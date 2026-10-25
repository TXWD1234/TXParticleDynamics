// Copyright (c) 2026 TXParticleDynamics. Licensed under the MIT License.

#pragma once
#include "impl/basic_storage.hpp"
#include "impl/data_utils.hpp"
#include "infrastructure.hpp"
#include "impl/geom_utils.hpp"
#include "impl/geometry.hpp"
#include "impl/numeric_utils.hpp"
#include "tx/type_traits.hpp"
#include <atomic>
#include <bit>
#include <bits/stdc++.h>
#include <cmath>
#include <exception>
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
	f32 ObjectRadius = 0.008f;
};
// all configuration that are not required to construct the engine
struct EngineConfig {
	f32 Gravity = 0.0001f;
	f32 Friction = 0.999f;
	f32 Restitution = 0.9f;
};

class Engine {
public:
	Engine() = default;
	Engine(EngineParameters param, EngineConfig config)
	    : m_param(param.SubstepCount, param.ObjectRadius,
	              lastPowTwo(static_cast<u32>(std::ceil(1.0 / param.ObjectRadius)))),
	      m_data(tx::AlternatingBuffer<vec2>(param.MaxEntityCount),
	             tx::AlternatingBuffer<vec2>(param.MaxEntityCount)),
	      m_config(config), m_threads(param.ThreadCount),
	      m_gs(param.MaxEntityCount, m_param.GridSideCellCount,
	           m_threads, GridSystemBufferSorter_impl{ &m_data }) {}

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

	// determinded at construction
	struct Parameters_impl {
		u32 SubstepCount;
		f32 ObjectRadius;
		u32 GridSideCellCount;
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
	// ################ Grid System Sorter ################
	// Sort the arrays according to the result of grid system update
	// for grid system construct secondary sort

	struct GridSystemBufferSorter_impl {
		Data_impl* m_data;
		void operator()(u32 prev, u32 curr) {
			m_data->pos.buffer(1)[curr] = m_data->pos.buffer(0)[prev];
			m_data->prevPos.buffer(1)[curr] = m_data->prevPos.buffer(0)[prev];
		}
	};

private:
	// ################ Concurrency ################

	struct TaskGSConstructSourceGridPos {
		struct Argument {
			const vec2* src;
			u32* dest;
			u32 size;
			u32 SideCellCount; // must be pow2
		};
		template <class... Args>
		void operator()(std::variant<Args...>& varg) {
			Argument& arg = std::get<Argument>(varg);
			for (u32 i = 0; i < arg.size; i++) {
				uvec2 gridPos = findGridPos(*(arg.src + i), arg.SideCellCount);
				u32 xSentinel = findSentinel(gridPos.x, arg.SideCellCount);
				u32 ySentinel = findSentinel(gridPos.y, arg.SideCellCount);
				(*(arg.dest + i)) =
				    tx::ZGridToIndex(uvec2(
				        gridPos.x | xSentinel | ySentinel,
				        gridPos.y | xSentinel | ySentinel));
			}
		}
		static uvec2 findGridPos(vec2 pos, f32 SideCellCount) {
			return tx::toCoord<u32>((pos + vec2(1.0f)) * 0.5f * SideCellCount);
		}
		// check if v as a x/y coordinate is out of bound
		// produces 0b1111111111111111 when out of bound
		//         0b10000000000000000 otherwise
		static u32 findSentinel(u32 v, u32 SideCellCount) {
			return (u32{ 1 } << 16) - bool(v >> std::bit_width(SideCellCount - 1));
		}
		/**
		 * Handling out-of-range particles
		 * The grid system only spans the range of [-1, 1), and any particles
		 * with position outisde of that range are considered out-of-range.
		 * The policy chosen for out-of-range particles is simply ignore them.
		 * 
		 * To avoid branching, the above bit operation abomination was created.
		 * The invariant it produces is: When a particle is out-of-range, its
		 * grid system index is exactly 0xFFFFFFFF.
		 */
	};

	struct TaskGSConstructSourceCompose {
		struct Argument {
			u32* destGridPos;
			u32* destObjectID;
			u32* src; // grid pos raw
			u32 srcSize;
			u32 idBase;
		};
		template <class... Args>
		void operator()(std::variant<Args...>& varg) {
			Argument& arg = std::get<Argument>(varg);
			u32 dest = 0;
			for (u32 i = 0; i < arg.srcSize; i++) {
				// arg.destGridPos[dest] = arg.src[i];
				// arg.destObjectID[dest] = i + arg.idBase;
				// dest += arg.src[i] != u32{ 0xFFFFFFFF };
				if (arg.src[i] != u32{ 0xFFFFFFFF }) {
					arg.destGridPos[dest] = arg.src[i];
					arg.destObjectID[dest] = i + arg.idBase;
					dest++;
				}
			}
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
				u32 old = acc;
				acc += arg.dest[i];
				arg.dest[i] = old;
			}
		}
	};

	struct TaskGSConstructPrimarySortSort {
		struct Argument {
			u32 RadixDigit;
			u32* indexArray; // BucketIndex
			u32* src; // gridPos, size = size
			u32* dest; // Bucket, size = size
			u32* id; // ObjectID, size = size
			u32 size;
			u32* idSrc; // sourceObjectID
		};
		template <class... Args>
		void operator()(std::variant<Args...>& varg) {
			Argument& arg = std::get<Argument>(varg);
			for (u32 i = 0; i < arg.size; i++) {
				u32 radix = GridSystem_impl::radixLowerBits(
				    arg.src[i], arg.RadixDigit);
				u32 index = arg.indexArray[radix]++;
				arg.dest[index] = GridSystem_impl::radixHigherBits(
				    arg.src[i], arg.RadixDigit);
				arg.id[index] = arg.idSrc[i];
			}
		}
	};

	struct TaskGSConstructSecondarySortBucketElementCount {
		struct Argument {
			u32 RadixDigit;
			u32 RadixBucketCount;
			u32* index; // root of primarySortBucketIndex
			u32 indexStrideOffset;
			u32 indexStrideSize; // distribution
			u32 indexStrideCount; // thread count
			u32* src; // root of primary sort bucket
			u32* dest; // size = RadixBucketCount; BucketIndex
			u32* indexDest; // root of grid system indexArray
		};
		template <class... Args>
		void operator()(std::variant<Args...>& varg) {
			Argument& arg = std::get<Argument>(varg);
			std::fill(arg.dest, arg.dest + arg.RadixBucketCount, 0);
			// counting
			// bucket is also the lower bits of the original grid index
			for (u32 bucket = arg.indexStrideOffset;
			     bucket < arg.indexStrideSize + arg.indexStrideOffset;
			     bucket++) {
				// for each stride
				for (u32 stride = 0; stride < arg.indexStrideCount; stride++) {

					// for every bucket in index
					u32 primaryIndex = bucket + stride * arg.RadixBucketCount;

					for (u32 j = arg.index[primaryIndex];
					     j < arg.index[primaryIndex + 1]; j++) {
						// for every element in the bucket

						arg.dest[arg.src[j]]++; // counting secondary bucket size
						// counting gridPos occurance
						// constructing grid system indexArray
						arg.indexDest[(arg.src[j] << arg.RadixDigit) | bucket]++;
					}
				}
			}
		}
	};

	struct TaskGSConstructSecondarySortBucketIndexSum {
		struct Argument {
			u32 ThreadCount;
			u32 RadixBucketCount;
			u32* src; // first entry in first stride in BucketIndex
			u32 srcStrideSize;
			u32* res; // bucket index sum (with + 1 pffset)
		};
		template <class... Args>
		void operator()(std::variant<Args...>& varg) {
			Argument& arg = std::get<Argument>(varg);
			for (u32 bucket = 0; bucket < arg.srcStrideSize; bucket++) {
				u32 acc = 0;
				for (u32 i = 0; i < arg.ThreadCount; i++) {
					acc += *(arg.src + bucket + i * arg.RadixBucketCount);
				}
				*(arg.res + bucket) = acc;
			}
		}
	};

	struct TaskGSConstructSecondarySortBucketIndexPSA {
		struct Argument {
			u32 ThreadCount;
			u32 RadixBucketCount;
			u32* src; // first entry in first stride in BucketIndex
			u32 srcStrideSize;
			u32* sum; // bucket index sum (without offset)
		};
		template <class... Args>
		void operator()(std::variant<Args...>& varg) {
			Argument& arg = std::get<Argument>(varg);
			for (u32 bucket = 0; bucket < arg.srcStrideSize; bucket++) {
				u32 acc = arg.sum[bucket];
				for (u32 i = 0; i < arg.ThreadCount; i++) {
					u32 old = acc;
					acc += *(arg.src + bucket + i * arg.RadixBucketCount);
					*(arg.src + bucket + i * arg.RadixBucketCount) = old;
				}
			}
		}
	};

	struct TaskGSConstructSecondarySortSort {
		struct Argument {
			u32 RadixBucketCount;
			u32* primaryIndex; // first stride in primarySortBucketIndex
			u32 primaryIndexStrideSize; // distribution
			u32 primaryIndexStrideCount; // thread count
			u32* src; // root of primary sort bucket
			u32* id; // root of primary object id
			u32* index; // BucketIndex
			GridSystemBufferSorter_impl sorter;
		};
		template <class... Args>
		void operator()(std::variant<Args...>& varg) {
			Argument& arg = std::get<Argument>(varg);
			u32 index = 0; // index in primary index array
			while (arg.primaryIndexStrideCount--) {
				u32 begin = index;
				index += arg.primaryIndexStrideSize; // index is now end

				for (u32 i = arg.primaryIndex[begin]; i < arg.primaryIndex[index]; i++) {
					// for each element in primary sort bucket
					arg.sorter(arg.id[i], arg.index[arg.src[i]]++);
				}

				index += arg.RadixBucketCount;
			}
		}
	};

	struct TaskGSConstructSecondarySortIndexArraySum {
		struct Argument {
			u32* src;
			u32 srcSize;
			u32* res;
		};
		template <class... Args>
		void operator()(std::variant<Args...>& varg) {
			Argument& arg = std::get<Argument>(varg);
			u32 sum = 0;
			for (u32 i = 0; i < arg.srcSize; i++) {
				sum += arg.src[i];
			}
			*arg.res = sum;
		}
	};

	struct TaskGSConstructSecondarySortIndexArrayPSA {
		struct Argument {
			u32* src;
			u32 srcSize;
			u32 psaBase;
		};
		template <class... Args>
		void operator()(std::variant<Args...>& varg) {
			Argument& arg = std::get<Argument>(varg);
			u32 acc = arg.psaBase;
			for (u32 i = 0; i < arg.srcSize; i++) {
				u32 old = acc;
				acc += arg.src[i];
				arg.src[i] = old;
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
	    TaskGSConstructSourceGridPos,
	    TaskGSConstructSourceCompose,
	    TaskGSConstructPrimarySortBucketIndex,
	    TaskGSConstructPrimarySortSort,
	    TaskGSConstructSecondarySortBucketElementCount,
	    TaskGSConstructSecondarySortBucketIndexSum,
	    TaskGSConstructSecondarySortBucketIndexPSA,
	    TaskGSConstructSecondarySortSort,
	    TaskGSConstructSecondarySortIndexArraySum,
	    TaskGSConstructSecondarySortIndexArrayPSA>;

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
		return isColliding(m_data.pos[a], m_data.pos[b], m_param.ObjectRadius * 2);
	}

private:
	// ################ Grid System ################

	class GridSystem_impl {
	public:
		// ================ Task Utility ================

		static u32 radixLowerBits(u32 val, u32 RadixDigit) {
			return val & ((u32{ 1 } << RadixDigit) - 1);
		}
		static u32 radixHigherBits(u32 val, u32 RadixDigit) {
			return val >> RadixDigit;
		}

	public:
		// ================ Lifetime ================

		GridSystem_impl() = default;
		GridSystem_impl(u32 MaxEntityCount, u32 SideCellCount,
		                std::span<Thread> threads,
		                GridSystemBufferSorter_impl sorter)
		    : m_source({ {}, threads, Distributor(threads), sorter }) {
			initState_impl(MaxEntityCount, SideCellCount);
			initBuffers_impl(MaxEntityCount, SideCellCount, threads.size());
		}

	public:
		// ================ Public Accessor ================

		template <class T>
		tx::PackedPartedView<T> view(T* ptr) {
			return tx::PackedPartedView<T>(
			    ptr, indexArray.data(), indexArray.size());
		}

	public:
		// ================ Public Interface ================

		void update(std::span<const vec2> posBuffer) {
			m_source.pos = posBuffer;
			construct_impl();
		}

	private:
		// ================ Grid System Data ================

		std::vector<u32> indexArray;

	private:
		// ================ Construction Data ================

		struct Source_impl {
			std::span<const vec2> pos;
			std::span<Thread> threads;
			Distributor distor;
			GridSystemBufferSorter_impl sorter;
		} m_source;

		struct State_impl {
			u32 SideCellCount = 0;
			u32 RadixDigit = 0;
			u32 RadixBucketCount = 0;
			std::unique_ptr<std::atomic<u32>> threadStatusCounter =
			    std::make_unique<std::atomic<u32>>(0);
		} m_state;

		struct Data_impl {
			// dynamic buffer that consist a element count member
			template <class T>
			struct DBuffer : public tx::BasicStorage<T> {
				using BasicStorage<T>::BasicStorage;
				u32 count = 0;
			};
			template <class T>
			using SBuffer = tx::BasicStorage<T>;
			// constant buffer init once at startup
			template <class T>
			using CBuffer = tx::BasicStorage<T>;

			// when reading names in here, add an imaginary "Array" suffix

			SBuffer<u32> sourcePSABase;
			DBuffer<u32> sourceGridPosRaw;
			DBuffer<u32> sourceGridPos;
			DBuffer<u32> sourceObjectID;
			SBuffer<u32> primarySortBucket; // {Buckets}[ThreadCount]
			SBuffer<u32> primarySortObjectID; // parallel to primarySortBucket
			// [RadixBucketCount][ThreadCount]
			SBuffer<u32> primarySortBucketIndex; // store global index in bucket
			SBuffer<u32> secondarySortBucketIndex;
			SBuffer<u32> secondarySortBucketIndexSum;
			CBuffer<u32> secondarySortDistribution; // per thread in bucket index
			SBuffer<u32> secondarySortIndexArraySum;

			/**
			 * primarySortBucket and primarySortObjectID have dynamic size, but
			 * because they are always accessed via index array, their size
			 * does not matter, therefore are SBuffer but not DBuffer.
			 */
		} m_data;

		void initState_impl(u32 n, u32 gn) {
			m_state.SideCellCount = gn;
			m_state.RadixBucketCount = gn;
			m_state.RadixDigit = std::bit_width(sq(gn) - 1) / 2;
		}
		void initBuffers_impl(u32 n, u32 gn, u32 t) {
			indexArray.resize(sq(gn) + 1);
			m_data.sourcePSABase = Data_impl::SBuffer<u32>(t + 1);
			m_data.sourcePSABase.data[0] = 0;
			m_data.sourceGridPosRaw = Data_impl::DBuffer<u32>(n);
			m_data.sourceGridPos = Data_impl::DBuffer<u32>(n);
			m_data.sourceObjectID = Data_impl::DBuffer<u32>(n);
			m_data.primarySortBucket = Data_impl::DBuffer<u32>(n);
			m_data.primarySortObjectID = Data_impl::DBuffer<u32>(n);
			m_data.primarySortBucketIndex = Data_impl::SBuffer<u32>(
			    m_state.RadixBucketCount * t + 2);
			m_data.primarySortBucketIndex.data[0] = 0;
			m_data.secondarySortBucketIndex = Data_impl::SBuffer<u32>(
			    m_state.RadixBucketCount * t);
			m_data.secondarySortBucketIndexSum = Data_impl::SBuffer<u32>(
			    m_state.RadixBucketCount + 1);
			m_data.secondarySortDistribution = Data_impl::CBuffer<u32>(t);
			m_data.secondarySortIndexArraySum = Data_impl::SBuffer<u32>(t);
		}
		void initConstBuffers_impl() {
			tx::distributePartitionsAligned(
			    m_state.RadixBucketCount, m_source.threads.size(),
			    tx::CacheLineCapacity<u32>,
			    [this](u32 begin, u32 end, u32 tIndex) {
				    m_data.secondarySortDistribution.data[tIndex] = end - begin;
			    });
		}

	private:
		// ================ Helpers ================

		// DevNote: Switch to std::latch for the entire fence system
		// Func: threaded directly to tx::distributePartitions
		template <class Func>
		void dispatchProcess(u32 size, Func&& f) {
			threadRunFencedProcess_impl(
			    m_state.threadStatusCounter, m_source.threads.size(), [this, &f, size] {
				    tx::distributePartitions(
				        size, m_source.threads.size(),
				        std::forward<Func>(f));
			    });
		}
		template <class Func>
		void dispatchProcessAligned(u32 size, u32 alignment, Func&& f) {
			threadRunFencedProcess_impl(
			    m_state.threadStatusCounter, m_source.threads.size(),
			    [this, &f, size, alignment] {
				    tx::distributePartitionsAligned(
				        size, m_source.threads.size(),
				        alignment, std::forward<Func>(f));
			    });
		}

	private:
		// ================ GS Construction Logic ================

		void construct_impl() {
			m_source.distor.reset();
			constructSource_impl();
			constructPrimarySort_impl();
			constructSecondarySort_impl();
		}

		void constructSource_impl() {
			constructSourceGridPosRaw_impl();
			constructSourceMeta_impl();
			constructSourceCompose_impl();
		}
		void constructSourceGridPosRaw_impl() {
			m_data.sourceGridPosRaw.count = m_source.pos.size();
			dispatchProcess(
			    m_source.pos.size(), [this](u32 begin, u32 end) {
				    threadAssignFenced_impl<TaskGSConstructSourceGridPos>(
				        m_source.distor, m_state.threadStatusCounter,
				        TaskGSConstructSourceGridPos::Argument{
				            m_source.pos.data() + begin,
				            m_data.sourceGridPosRaw.data + begin, end - begin,
				            m_state.SideCellCount });
			    });
		}
		void constructSourceMeta_impl() {
			tx::distributePartitions(
			    m_data.sourceGridPosRaw.count, m_source.threads.size(),
			    [this](u32 begin, u32 end, u32 tIndex) {
				    // count valid grid pos in range and construct psa
				    tIndex++;
				    m_data.sourcePSABase.data[tIndex] =
				        m_data.sourcePSABase.data[tIndex - 1];
				    for (u32 i = begin; i < end; i++) {
					    m_data.sourcePSABase.data[tIndex] +=
					        m_data.sourceGridPosRaw.data[i] != u32{ 0xFFFFFFFF };
				    }
			    });
			m_data.sourceGridPos.count =
			    m_data.sourcePSABase.data[m_data.sourcePSABase.size - 1];
			m_data.sourceObjectID.count = m_data.sourceGridPos.count;
		}
		void constructSourceCompose_impl() {
			dispatchProcess(
			    m_data.sourceGridPosRaw.count, [this](u32 begin, u32 end, u32 tIndex) {
				    threadAssignFenced_impl<TaskGSConstructSourceCompose>(
				        m_source.distor, m_state.threadStatusCounter,
				        TaskGSConstructSourceCompose::Argument{
				            m_data.sourceGridPos.data + m_data.sourcePSABase.data[tIndex],
				            m_data.sourceObjectID.data + m_data.sourcePSABase.data[tIndex],
				            m_data.sourceGridPosRaw.data + begin, end - begin, begin });
			    });
		}

		void constructPrimarySort_impl() {
			constructPrimarySortBucketIndex_impl();
			constructParimarySortSort_impl();
		}
		// counting
		void constructPrimarySortBucketIndex_impl() {
			dispatchProcess(
			    m_data.sourceGridPos.count, [this](u32 begin, u32 end, u32 tIndex) {
				    threadAssignFenced_impl<TaskGSConstructPrimarySortBucketIndex>(
				        m_source.distor, m_state.threadStatusCounter,
				        TaskGSConstructPrimarySortBucketIndex::Argument{
				            m_state.RadixDigit, m_state.RadixBucketCount,
				            begin, m_data.sourceGridPos.data + begin, end - begin,
				            m_data.primarySortBucketIndex.data + 1 +
				                tIndex * m_state.RadixBucketCount });
			    });
		}
		// sorting
		void constructParimarySortSort_impl() {
			dispatchProcess(
			    m_data.sourceGridPos.count, [this](u32 begin, u32 end, u32 tIndex) {
				    threadAssignFenced_impl<TaskGSConstructPrimarySortSort>(
				        m_source.distor, m_state.threadStatusCounter,
				        TaskGSConstructPrimarySortSort::Argument{
				            m_state.RadixDigit,
				            m_data.primarySortBucketIndex.data + 1 +
				                tIndex * m_state.RadixBucketCount,
				            m_data.sourceGridPos.data + begin,
				            m_data.primarySortBucket.data + begin,
				            m_data.primarySortObjectID.data + begin,
				            end - begin, m_data.sourceObjectID.data + begin });
			    });
		}

		/**
		 * The transformation of m_data.primarySortBucketIndex array
		 * Upon close inspection, a consistent pattern appears during primary
		 * sort: all references to primary sort bucket index duuring primary
		 * sort has an offset of 1. which means there's an untouched slot at
		 * the very start of the bucket index array with constant value of 0.
		 * After primary sort, all indices in primary bucket index array
		 * becomes end, which means the entire thing effectively shifted left
		 * by one slot. and now in secondary sort, all references to primary
		 * sort are without the 1 slot offset, which means now that one slot
		 * at the front became the one slot erased by previously shifting it
		 * to left by one slot. and the value of that slot is always 0, since
		 * it's an exclusive psa. therefore during the secondary sort, the
		 * primary bucket index is still a perfect exlusive psa.
		 */

		void constructSecondarySort_impl() {
			constructSecondarySortBucketElementCount_impl();
			constructSecondarySortBucketIndex_impl();
			constructSecondarySortSort_impl();
			constructSecondarySortIndexArray_impl();
		}
		/**
		 * Counts the element of each thread partition.
		 * After this process, m_data.secondarySortBucketIndex contains the
		 * element count of each thread in each bucket. Note that the buckets
		 * now are secondary bucket, aka high bits.
		 * 
		 * Also constructs grid system indexArray.
		 * False sharing when writing into indexArray is avoided by deliberate
		 * design: Each thread handles a certain count of complete buckets from primary
		 * sort, which are also the lower bits of the grid index. Which means,
		 * every thread writes to contiguous strides in indexArray. And by
		 * partitioning the buckets such that every thread process bucket count
		 * that are multiples of a cache line size, false sharing is completely
		 * avoided.
		 */
		void constructSecondarySortBucketElementCount_impl() {
			std::fill(indexArray.begin(), indexArray.end(), 0);
			threadRunFencedProcess_impl(
			    m_state.threadStatusCounter, m_source.threads.size(),
			    [this] {
				    u32 begin = 0;
				    for (u32 i = 0; i < m_data.secondarySortDistribution.size; i++) {
					    threadAssignFenced_impl<TaskGSConstructSecondarySortBucketElementCount>(
					        m_source.distor, m_state.threadStatusCounter,
					        TaskGSConstructSecondarySortBucketElementCount::Argument{
					            m_state.RadixDigit, m_state.RadixBucketCount,
					            m_data.primarySortBucketIndex.data, begin,
					            m_data.secondarySortDistribution.data[i],
					            m_data.secondarySortDistribution.size,
					            m_data.primarySortBucket.data,
					            m_data.secondarySortBucketIndex.data +
					                i * m_state.RadixBucketCount,
					            indexArray.data() });
					    begin += m_data.secondarySortDistribution.data[i];
				    }
			    });
		}
		/**
		 * Using the element count data from
		 * constructSecondarySortBucketElementCount_impl, this process compose
		 * the actual bucket index array by building a stride psa
		 */
		void constructSecondarySortBucketIndex_impl() {
			threadRunFencedProcess_impl(
			    m_state.threadStatusCounter, m_source.threads.size(),
			    [this] {
				    u32 begin = 0;
				    for (u32 i = 0; i < m_data.secondarySortDistribution.size; i++) {
					    threadAssignFenced_impl<TaskGSConstructSecondarySortBucketIndexSum>(
					        m_source.distor, m_state.threadStatusCounter,
					        TaskGSConstructSecondarySortBucketIndexSum::Argument{
					            static_cast<u32>(m_source.threads.size()),
					            m_state.RadixBucketCount,
					            m_data.secondarySortBucketIndex.data + begin,
					            m_data.secondarySortDistribution.data[i],
					            m_data.secondarySortBucketIndexSum.data + begin });
					    begin += m_data.secondarySortDistribution.data[i];
				    }
			    });
			threadRunFencedProcess_impl(
			    m_state.threadStatusCounter, m_source.threads.size(),
			    [this] {
				    u32 begin = 0;
				    for (u32 i = 0; i < m_data.secondarySortDistribution.size; i++) {
					    threadAssignFenced_impl<TaskGSConstructSecondarySortBucketIndexPSA>(
					        m_source.distor, m_state.threadStatusCounter,
					        TaskGSConstructSecondarySortBucketIndexPSA::Argument{
					            static_cast<u32>(m_source.threads.size()),
					            m_state.RadixBucketCount,
					            m_data.secondarySortBucketIndex.data + begin,
					            m_data.secondarySortDistribution.data[i],
					            m_data.secondarySortBucketIndexSum.data + begin });
					    begin += m_data.secondarySortDistribution.data[i];
				    }
			    });
		}
		void constructSecondarySortSort_impl() {
			threadRunFencedProcess_impl(
			    m_state.threadStatusCounter, m_source.threads.size(),
			    [this] {
				    u32 begin = 0;
				    for (u32 i = 0; i < m_data.secondarySortDistribution.size; i++) {
					    threadAssignFenced_impl<TaskGSConstructSecondarySortSort>(
					        m_source.distor, m_state.threadStatusCounter,
					        TaskGSConstructSecondarySortSort::Argument{
					            m_state.RadixBucketCount,
					            m_data.primarySortBucketIndex.data + begin,
					            m_data.secondarySortDistribution.data[i],
					            static_cast<u32>(m_data.secondarySortDistribution.size),
					            m_data.primarySortBucket.data,
					            m_data.primarySortObjectID.data,
					            m_data.secondarySortBucketIndex.data +
					                i * m_state.RadixBucketCount,
					            m_source.sorter });
					    begin += m_data.secondarySortDistribution.data[i];
				    }
			    });
		}
		// construct psa of index array
		void constructSecondarySortIndexArray_impl() {
			dispatchProcessAligned(
			    indexArray.size(), tx::CacheLineCapacity<u32>,
			    [this](u32 begin, u32 end, u32 tIndex) {
				    threadAssignFenced_impl<
				        TaskGSConstructSecondarySortIndexArraySum>(
				        m_source.distor, m_state.threadStatusCounter,
				        TaskGSConstructSecondarySortIndexArraySum::Argument{
				            indexArray.data() + begin, end - begin,
				            m_data.secondarySortIndexArraySum.data + tIndex });
			    });
			u32 acc = 0;
			for (u32 i = 0; i < m_data.secondarySortIndexArraySum.size; i++) {
				u32 old = acc;
				acc += m_data.secondarySortIndexArraySum.data[i];
				m_data.secondarySortIndexArraySum.data[i] = old;
			}
			dispatchProcessAligned(
			    indexArray.size(), tx::CacheLineCapacity<u32>,
			    [this](u32 begin, u32 end, u32 tIndex) {
				    threadAssignFenced_impl<
				        TaskGSConstructSecondarySortIndexArrayPSA>(
				        m_source.distor, m_state.threadStatusCounter,
				        TaskGSConstructSecondarySortIndexArrayPSA::Argument{
				            indexArray.data() + begin, end - begin,
				            *(m_data.secondarySortIndexArraySum.data + tIndex) });
			    });
			indexArray.back() = m_source.pos.size();
		}


	} m_gs;

private:
	// ################ Grid System Debugging ################

	void checkGSValid_impl() {
		tx::PackedPartedView view = m_gs.view(m_data.pos.data());
		for (u32 i = 0; i < view.partition_size(); i++) {
			std::span<vec2> s = view[i];
			for (vec2 v : s) {
				if (i != tx::ZGridToIndex(TaskGSConstructSourceGridPos::findGridPos(
				             v, m_param.GridSideCellCount))) {
					std::cerr << "[Error]: GS check failed!" << std::endl;
					std::terminate();
				}
			}
		}
	}

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
		f32 overlap = (m_param.ObjectRadius * 2 - dist) * 0.5f;
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
 * - [P02] Ensure all buffer's Cache line alignment
 * - [P03] Arena and huge page
 * - [P04] Parallelize meta construction
 * - [P05] Branchless TaskGSConstructSourceCompose / AVX-512 vpcompressd
 * Grid System:
 * - [P01] (adopted) instead of making a grid system array, rearrange the position array
 *   directly. Effectively sorting the position buffer.
 */