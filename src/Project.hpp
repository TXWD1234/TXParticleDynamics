// Copyright (c) 2026 TXParticleDynamics. Licensed under the MIT License.

#pragma once
#include "infrastructure.hpp"
#include "impl/geom_utils.hpp"
#include "impl/geometry.hpp"
#include "impl/numeric_utils.hpp"
#include "tx/type_traits.hpp"
#include <atomic>
#include <bit>
#include <bits/stdc++.h>
#include <cmath>
#include <memory>
#include <type_traits>

// ########################################
// $$$$$$$$$$$$$$$$ Engine $$$$$$$$$$$$$$$$
// ########################################
/**
 * WE LOVE VERLET
 */

namespace tx::ParticleDynamics {

// all configuration that are required to construct the engine
struct EngineParameters {
	u32 MaxEntityCount = 10000;
	u32 SubstepCount = 8;
	u32 ThreadCount = 20;
};
// all configuration that are not required to construct the engine
struct EngineConfig {
	f32 ObjectRadius = 0.008f;
	f32 Gravity = 0.0001f;
	f32 Friction = 0.999f;
	f32 Restitution = 0.9f;
};

class Engine {
public:
	Engine() = default;
	Engine(EngineParameters param, EngineConfig config)
	    : m_param(param.SubstepCount),
	      m_data(tx::AlternatingBuffer<vec2>(param.MaxEntityCount),
	             tx::AlternatingBuffer<vec2>(param.MaxEntityCount)),
	      m_config(config), m_threads(param.ThreadCount),
	      m_gs(param.MaxEntityCount, m_threads) {}

public:
	// ################ Main Loop Interface ################

	void update() { update_impl(m_param.SubstepCount); }

	template <std::invocable<const std::span<vec2>> Func>
	void draw(Func&& f) {
		f(std::span(m_data.pos.begin(), m_data.pos.begin() + m_state.objectCount));
	}

public:
	// ################ Public Interface ################

	EngineConfig& config() { return m_config; }

	u32 createObject(vec2 pos, vec2 initVel = tx::Origin) {
		m_data.pos[m_state.objectCount] = pos;
		m_data.prevPos[m_state.objectCount] = pos - initVel;
		m_state.objectCount++;
		return m_state.objectCount - 1;
	}

	void applyImpulse(vec2 pos, f32 radius, f32 intensity) {
		for (u32 i = 0; i < m_state.objectCount; i++) {
			if (isColliding(m_data.pos[i], pos, radius)) {
				vec2 nuv = tx::normalize(m_data.pos[i] - pos);
				m_data.pos[i] += nuv * intensity;
			}
		}
	}

	// default to clock-wise. set magnitude to negative for counter-clockwise
	void applyRotationalForce(f32 magnitude, vec2 center = tx::Origin) {
		for (u32 i = 0; i < m_state.objectCount; i++)
			applyVel_impl(i, tx::rightPerp(tx::normalize(center - m_data.pos[i])) * magnitude);
	}

private:
	// ################ Runtime Data ################

	struct Parameters_impl {
		u32 SubstepCount;
	} m_param;

	struct Data_impl {
		tx::AlternatingBuffer<vec2> pos;
		tx::AlternatingBuffer<vec2> prevPos;
		// std::vector<u32> ids; // ID of objects to adapt sorting
	} m_data;

	struct State_impl {
		u32 objectCount = 0;
	} m_state;

	EngineConfig m_config;

private:
	// ================ Concurrency ================

	struct TaskGSConstructGridPos {
		struct Argument {
			const vec2* src;
			u32* dest;
			u32 size;
			u32 SideCellCount;
		};
		template <class... Args>
		void operator()(std::variant<Args...>& varg) {
			Argument& arg = std::get<Argument>(varg);
			for (u32 i = 0; i < arg.size; i++) {
				(*(arg.dest + i)) =
				    tx::ZGridToIndex(findGridPos(*(arg.src + i), arg.SideCellCount));
			}
		}
		uvec2 findGridPos(vec2 pos, f32 SideCellCount) {
			return tx::toCoord<u32>((pos + vec2(1.0f)) * 0.5f * SideCellCount);
		}
	};

	struct TaskGSConstructPrimarySortBucketIndex {
		struct Argument {
			u32 RadixDigit;
			u32 RadixBucketCount;
			u32 begin; // psa base
			u32* src; // gridPos
			u32 srcSize;
			u32* dest; // size = RadixBucketCount; BucketIndex
		};
		template <class... Args>
		void operator()(std::variant<Args...>& varg) {
			Argument& arg = std::get<Argument>(varg);
			// collect count of each bucket (DevNote: optimization: unroll & SIMD)
			std::fill(arg.dest, arg.dest + arg.RadixBucketCount, 0);
			for (u32 i = 0; i < arg.srcSize; i++) {
				arg.dest[GridSystem_impl::radixLowerBits(arg.src[i], arg.RadixDigit)]++;
			}
			// construct psa (SIMDable)
			u32 acc = arg.begin;
			for (u32 i = 0; i < arg.RadixBucketCount; i++) {
				acc += arg.dest[i];
				arg.dest[i] = acc;
			}
		}
	};

	struct TaskFence {
		struct Argument {
			std::atomic<u32>* counter;
		};
		template <class... Args>
		void operator()(std::variant<Args...>& varg) {
			Argument& arg = std::get<Argument>(varg);
			if (arg.counter->fetch_sub(1, std::memory_order_acq_rel) == 1)
				arg.counter->notify_one();
		}
	};

	using TaskList = tx::type_list_t<
	    TaskFence,
	    TaskGSConstructGridPos,
	    TaskGSConstructPrimarySortBucketIndex>;

	template <class T>
	using ThreadArgumentExtract_t = typename T::Argument;
	using ThreadArgument = tx::type_list_apply_t<
	    tx::type_list_transform_t<TaskList, ThreadArgumentExtract_t>,
	    std::variant>;

	using Thread = typename decltype(TaskList::apply(
	    []<class... Args>(tx::type_list_t<Args...>) {
		    return std::type_identity<TaskThread<ThreadArgument, Args...>>{};
	    }))::type;

	using Distributor = TaskThreadDistributor<ThreadArgument, Thread>;

	std::vector<Thread> m_threads;

	// ================ Concurrency Helpers ================

	// assign with fences
	template <class... Func>
	static void threadAssignFenced_impl(Distributor& distor,
	                                    std::unique_ptr<std::atomic<u32>>& counter,
	                                    auto... args) {
		distor.assign<Func..., TaskFence>(
		    args..., TaskFence::Argument{ counter.get() });
	}

	template <class Func>
	static void threadRunFencedProcess_impl(std::unique_ptr<std::atomic<u32>>& counter,
	                                        u32 threadCount, Func&& f) {
		counter->store(threadCount, std::memory_order_release);
		f();
		u32 count = counter->load(std::memory_order_acquire);
		while (count != 0) {
			counter->wait(count, std::memory_order_acquire);
			count = counter->load(std::memory_order_acquire);
		}
	}

private:
	// ################ Implementation Utility ################

	vec2 findVel_impl(u32 id) { return m_data.pos[id] - m_data.prevPos[id]; }
	void applyVel_impl(u32 id, vec2 vel) { m_data.prevPos[id] -= vel; }

private:
	// ################ Physics Logic Utility ################

	bool isColliding(vec2 a, vec2 b, f32 dist) {
		return sq(a.x - b.x) + sq(a.y - b.y) < sq(dist);
	}
	bool isColliding(u32 a, u32 b) {
		return isColliding(m_data.pos[a], m_data.pos[b], m_config.ObjectRadius * 2);
	}

private:
	// ################ Grid System Sorter ################
	// Sort the arrays according to the result of grid system update

	struct GridSystemBufferSorter_impl {
		Data_impl* m_data;
		void operator()(u32 prev, u32 curr) {
			m_data->pos.buffer(1)[curr] = m_data->pos.buffer(0)[prev];
			m_data->prevPos.buffer(1)[curr] = m_data->prevPos.buffer(0)[prev];
		}
	};

private:
	// ################ Grid System ################

	class GridSystem_impl {
	public:
		// ================ Task Utility ================

		static u32 radixLowerBits(u32 val, u32 RadixDigit) {
			return val & (~0x0 << RadixDigit);
		}
		static u32 radixHigherBits(u32 val, u32 RadixDigit) {
			return val >> RadixDigit;
		}

	public:
		// ================ Lifetime ================

		GridSystem_impl() = default;
		GridSystem_impl(u32 MaxEntityCount, std::span<Thread> threads)
		    : m_source({ {}, threads, Distributor(threads) }) {
			constructState_impl(MaxEntityCount);
			constructBuffers_impl(MaxEntityCount);
		}

	public:
		// ================ Public Accessor ================

		tx::PackedPartedView<u32> view() {
			return tx::PackedPartedView<u32>(array.data(), indexArray.data());
		}

	public:
		// ================ Public Interface ================

		void update(std::span<const vec2> posBuffer) {
			m_source.pos = posBuffer;
			construct_impl();
		}

	private:
		// ================ Grid System Data ================

		std::vector<u32> array;
		std::vector<u32> indexArray;

	private:
		// ================ Update Buffers ================

		struct Source_impl {
			std::span<const vec2> pos;
			std::span<Thread> threads;
			Distributor distor;
		} m_source;

		struct State_impl {
			u32 SideCellCount = 0;
			u32 RadixDigit = 0;
			u32 RadixBucketCount = 0;
			std::unique_ptr<std::atomic<u32>> threadStatusCounter =
			    std::make_unique<std::atomic<u32>>(0);
		} m_state;

		struct Data_impl {
			// when reading names in here, add an imaginary "Array" suffix

			std::vector<u32> gridPos;
			std::vector<u32> primarySortBucket;
			std::vector<u32> primarySortObjectID;
			std::vector<u32> primarySortBucketIndex;
		} m_data;

		void constructState_impl(u32 n) {
			m_state.SideCellCount =
			    tx::nextPowTwo(static_cast<u32>(std::ceil(std::sqrt(n))));
			m_state.RadixDigit = std::bit_width(n) / 2;
			m_state.RadixBucketCount = tx::powi(2, m_state.RadixDigit);
		}
		void constructBuffers_impl(u32 n) {
			array.resize(n);
			indexArray.resize(n);
			m_data.gridPos.resize(n);
			m_data.primarySortBucket.resize(n);
			m_data.primarySortObjectID.resize(n);
			m_data.primarySortBucketIndex.resize(
			    m_state.RadixBucketCount * m_source.threads.size() + 1);
		}

	private:
		// ================ GS Construction Logic ================

		void construct_impl() {
			m_source.distor.reset();
			constructGridPos_impl();
		}

		void constructGridPos_impl() {
			threadRunFencedProcess_impl(
			    m_state.threadStatusCounter, m_source.threads.size(), [this] {
				    tx::distributePartitions(
				        m_source.pos.size(), m_source.threads.size(),
				        [this](u32 begin, u32 end) {
					        threadAssignFenced_impl<TaskGSConstructGridPos>(
					            m_source.distor, m_state.threadStatusCounter,
					            TaskGSConstructGridPos::Argument{
					                m_source.pos.data() + begin,
					                m_data.gridPos.data() + begin, end - begin,
					                m_state.SideCellCount });
				        });
			    });
		}

		void constructPrimarySort_impl() {
			constructPrimarySortBucketIndex_impl();
		}
		// counting
		void constructPrimarySortBucketIndex_impl() {
			threadRunFencedProcess_impl(
			    m_state.threadStatusCounter, m_source.threads.size(), [this] {
				    tx::distributePartitions(
				        m_data.gridPos.size(), m_source.threads.size(),
				        [this](u32 begin, u32 end, u32 tIndex) {
					        threadAssignFenced_impl<TaskGSConstructPrimarySortBucketIndex>(
					            m_source.distor, m_state.threadStatusCounter,
					            TaskGSConstructPrimarySortBucketIndex::Argument{
					                m_state.RadixDigit, m_state.RadixBucketCount,
					                begin, m_data.gridPos.data() + begin, end - begin,
					                m_data.primarySortBucketIndex.data() + 1 +
					                    tIndex * m_state.RadixBucketCount });
				        });
			    });
		}

	} m_gs;

private:
	// ################ Logic Implementation ################

	void update_impl(u32 substep) {
		f32 dt = 1.0f / static_cast<f32>(substep);
		while (substep--) {
			updateGravity_impl(dt);
			updateObject_impl();
			updateCollision_impl();
		}
	}

	// ================ Update Verlet ================

	void updateObject_impl() {
		for (u32 i = 0; i < m_state.objectCount; i++) {
			vec2 vel = findVel_impl(i);
			m_data.prevPos[i] = m_data.pos[i];
			m_data.pos[i] += vel * m_config.Friction;
		}
	}

	// ================ Update Gravity ================

	void updateGravity_impl(f32 dt) {
		for (u32 i = 0; i < m_state.objectCount; i++) {
			m_data.pos[i] +=
			    tx::normalize(tx::Origin - m_data.pos[i]) * m_config.Gravity * dt;
		}
	}

	// ================ Update Collision ================

	void updateCollision_impl() {
		for (u32 i = 0; i < m_state.objectCount; i++) {
			for (u32 j = i + 1; j < m_state.objectCount; j++) {
				if (isColliding(i, j))
					solveCollision_impl(i, j);
			}
		}
	}
	// Collision Solver, the core physics logic
	void solveCollision_impl(u32 a, u32 b) {
		vec2 diff = m_data.pos[a] - m_data.pos[b];
		f32 dist = tx::length(diff);
		vec2 nuv = tx::normalize(diff, dist);

		// push apart
		f32 overlap = (m_config.ObjectRadius * 2 - dist) * 0.5f;
		vec2 push = overlap * nuv;

		m_data.pos[a] += push;
		m_data.pos[b] -= push;

		// calculate impulse
		vec2 relVel = findVel_impl(a) - findVel_impl(b); // relative velocity
		f32 nRelVel = dot(relVel, nuv);

		if (nRelVel >= -0.0001f)
			return;
		// nRelVel = std::min(0.0f, nRelVel);

		// solve bounce
		f32 bounceImpulse = nRelVel * m_config.Restitution;
		vec2 impulseVec = nuv * bounceImpulse;
		m_data.prevPos[a] += impulseVec;
		m_data.prevPos[b] -= impulseVec;
	}
};

} // namespace tx::ParticleDynamics

/**
 * Optimization Idea:
 * - Ensure all buffer's Cache line alignment
 * Grid System:
 * - (adopted) instead of making a grid system array, rearrange the position array
 *   directly. Effectively sorting the position buffer.
 */