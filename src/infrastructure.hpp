// Copyright (c) 2026 TXParticleDynamics. Licensed under the MIT License.

#pragma once
#include "impl/allocator.hpp"
#include "impl/basic_storage.hpp"
#include "impl/data_utils.hpp"
#include "impl/geometry.hpp"
#include "impl/numeric_utils.hpp"
#include "tx/exception.hpp"
#include "tx/type_traits.hpp"
#include <atomic>
#include <concepts>
#include <memory>
#include <thread>
#include <type_traits>
#include <variant>

// ################################################
// $$$$$$$$$$$$$$$$ Infrasturcture $$$$$$$$$$$$$$$$
// ################################################

namespace tx {

// =============================================
// **************** Concurrency ****************
// =============================================

/**
 * Single-Producer-Single-Consumer Queue built for concurrency
 */
template <std::move_constructible T, tx::allocator Allocator = std::allocator<T>>
class SPSCQueue {
	/**
	 * Assumptions this data structures make:
	 * - push() is always called by a single thread (producer thread)
     * - pull() is always called by a single thread (consumer thread)
	 */

	/**
	 * Conventions this data structure expects user to follow:
	 * - The queue must be destroyed after the threads are joined. Implying the
	 *   queue to be declared before the thread in the parent class.
	 */

	/**
	 * For implementation details, the established convention is:
     * - Producer thread owns m_state.producer, and the end of the buffer
     * - Consumer thread owns m_state.consumer, and the front of the buffer
	 */

	/**
	 * To break pull() from deadlocking in empty queue, push a sentinel value.
	 */

public:
	SPSCQueue(u32 size, Allocator alloc = Allocator{}) {
		// Pow2 check for ring buffer
		if (!tx::isPowTwo(size)) {
			impl::assert_impl(
			    [] { return false; },
			    [=] {
				    return std::format(
				        "Bad construction. Argument `bufferSize` must be a "
				        "power of 2. bufferSize = {}",
				        size);
			    });
		} else {
			m_storage = tx::BasicStorage<T, Allocator>(size, alloc);
		}
	}

public:
	// ================ Public Interface ================

	template <class... Args>
	    requires std::constructible_from<T, Args...>
	void push(Args&&... args) {
		u32 end = m_state.producer.end.load(std::memory_order_relaxed);
		while (end - m_state.producer.begin >= m_storage.size) {
			u32 oldBegin = m_state.producer.begin;
			m_state.producer.begin =
			    m_state.consumer.begin.load(std::memory_order_acquire);
			m_state.producerSleeping.store(true, std::memory_order_seq_cst);
			m_state.consumer.begin.wait(oldBegin, std::memory_order_seq_cst);
			m_state.producerSleeping.store(false, std::memory_order_release);
		}

		std::construct_at(end_impl(end), std::forward<Args>(args)...);
		m_state.producer.end.store(end + 1, std::memory_order_seq_cst);
		// notify only if the consumer is currently sleeping and it gets the
		// it out of empty state
		if (m_state.consumerSleeping.load(std::memory_order_seq_cst))
			m_state.producer.end.notify_one();
	}

	T pull() {
		u32 begin = m_state.consumer.begin.load(std::memory_order_relaxed);
		while (m_state.consumer.end - begin == 0) {
			u32 oldEnd = m_state.consumer.end;
			m_state.consumer.end =
			    m_state.producer.end.load(std::memory_order_acquire);
			m_state.consumerSleeping.store(true, std::memory_order_seq_cst);
			m_state.producer.end.wait(oldEnd, std::memory_order_seq_cst);
			m_state.consumerSleeping.store(false, std::memory_order_release);
		}

		T* ptr = begin_impl(begin);
		T ret = std::move(*ptr);
		std::destroy_at(ptr);
		m_state.consumer.begin.store(begin + 1, std::memory_order_seq_cst);
		// notify only if the producer is currently sleeping and it gets the
		// it out of full state
		if (m_state.producerSleeping.load(std::memory_order_seq_cst))
			m_state.consumer.begin.notify_one();
		return ret;
	}

private:
	// ================ Ring Buffer Implementation ================
	// Re-implementing ring buffer here because access to the foundational
	// indices are required.

	alignas(CacheLineSize) struct {
		alignas(CacheLineSize) struct {
			std::atomic<u32> begin = 0;
			u32 end = 0; // cached value
		} consumer;
		alignas(CacheLineSize) std::atomic<bool> consumerSleeping = false;

		alignas(CacheLineSize) struct {
			std::atomic<u32> end = 0;
			u32 begin = 0; // cached value
		} producer;
		alignas(CacheLineSize) std::atomic<bool> producerSleeping = false;
	} m_state;

private:
	tx::BasicStorage<T, Allocator> m_storage;

private:
	template <class Self>
	tx::const_propagate<Self, T>* begin_impl(this Self&& self, u32 begin) {
		return self.m_storage.data +
		       impl::findPowTwoWrappedPhysIndex(begin, self.m_storage.size);
	}
	T* end_impl(u32 end) {
		return m_storage.data +
		       impl::findPowTwoWrappedPhysIndex(end, m_storage.size);
	}

	bool producer_empty_impl() {
		return m_state.producer.end.load(std::memory_order_relaxed) -
		           m_state.consumer.begin.load(std::memory_order_acquire) ==
		       0;
	}
};

template <class Argument, class... TArgs>
    requires(
        std::movable<Argument> && std::is_default_constructible_v<Argument> &&
        impl::ForwardLastAllocArgRemainder<TArgs...>::apply(
            []<class... Args>(tx::type_list_t<Args...>) {
	            return tx::satisfies_all<decltype([]<class Arg>(
	                                                  std::type_identity<Arg>) {
		                                     return std::invocable<Arg, Argument&>;
	                }),
	                Args...>;
            }))
class TaskThread {
	/**
	 * This class is design for non-trivial tasks. Pushing frequently with
	 * trivially done tasks is discouraged.
	 */

	/**
	  * DevNote: should I support stateful lambdas?
	  */
private:
	// ================ Type Traits ================

	using TaskFunc = typename tx::type_list_append_t<
	    impl::ForwardLastAllocArgRemainder<TArgs...>,
	    tx::Nothing>::template apply_t<std::variant>;
	struct Task {
		TaskFunc f;
		Argument arg;
	};
	using Allocator = impl::ParseLastAllocArg<std::allocator<Task>, TArgs...>;

private:
	// ================ Worker Function ================

	// @return continue or not
	bool executeTask_impl(Task task) {
		return std::visit(
		    [&](auto& f) {
			    if constexpr (
			        std::same_as<std::remove_cvref_t<decltype(f)>, tx::Nothing>)
				    return false;
			    else {
				    f(task.arg);
				    return true;
			    }
		    },
		    task.f);
	}

	void worker_impl() {
		while (executeTask_impl(m_queue.pull())) {}
	}

public:
	// ================ Lifetime ================

	TaskThread(u32 queueCapacity = 256, const Allocator& alloc = Allocator{})
	    : m_queue(queueCapacity, alloc), m_thread([this] { worker_impl(); }) {}

	/**
	 * Shutdown behavior: Finish executing all the queued tasks and then
	 * shutdown
	 */
	~TaskThread() {
		m_queue.push(Task{ TaskFunc{ tx::Nothing{} }, Argument{} });
		if (m_thread.joinable())
			m_thread.join();
	}

	TaskThread(const TaskThread& other) = delete;
	TaskThread& operator=(const TaskThread& other) = delete;
	TaskThread(TaskThread&& other) = default;
	TaskThread& operator=(TaskThread&& other) = default;

public:
	// ================ Public Interface ================

	// can only be called in one thread (producer thread)
	template <class... Func>
	    requires tx::satisfies_all<
	        decltype([]<class F>(std::type_identity<F>) {
		        return impl::ForwardLastAllocArgRemainder<TArgs...>::apply(
		            []<class... Tasks>(tx::type_list_t<Tasks...>) {
		        return tx::satisfies_any<
			                decltype([]<class Task>(std::type_identity<Task>) {
				                return std::same_as<Task, F>;
		            }),
			                Tasks...>;
		            });
	        }),
	        Func...>
	void assign(std::convertible_to<Argument> auto... arg)
	    requires(sizeof...(arg) == sizeof...(Func))
	{
		(m_queue.push(Task{ Func{}, std::move(arg) }), ...);
	}

private:
	// ================ Members ================

	SPSCQueue<Task, Allocator> m_queue;
	std::thread m_thread;
};


// =============================================
// **************** Grid System ****************
// =============================================

// ################ Z Grid ################
/**
 * Using the ZGrid / Z-order curve to accomplish maximum cache locality when
 * accessing adjacent grids.
 * Forcing the grid side count to be pow2 for parallel update stability.
 */

/**
 * Some macro and instrinsic abomination have to be used here for direct access
 * of hardware instruction, and cross platform portability for machines that
 * don't have the instruction
 */
} // namespace tx

#if defined(__BMI2__)
namespace tx {
constexpr bool PDEP_Available = true;
}
#define MACRO_PDEP_Available
#include <immintrin.h>
#else
namespace tx {
constexpr bool PDEP_Available = false;
}
#endif

namespace tx {
namespace intrin {
inline u32 pdep_u32(u32 x, u32 y) {
#ifdef MACRO_PDEP_Available
	return _pdep_u32(x, y);
#endif
}
inline u32 pext_u32(u32 x, u32 y) {
#ifdef MACRO_PDEP_Available
	return _pext_u32(x, y);
#endif
}
} // namespace intrin

inline u32 ZGridToIndex(uvec2 pos) {
	if constexpr (PDEP_Available)
		return intrin::pdep_u32(pos.x, 0x55555555) |
		       intrin::pdep_u32(pos.y, 0xAAAAAAAA);
	else {
		auto spread = [](u16 val) -> u32 {
			u32 v = val;
			v = (v | (v << 8)) & 0x00FF00FF;
			v = (v | (v << 4)) & 0x0F0F0F0F;
			v = (v | (v << 2)) & 0x33333333;
			v = (v | (v << 1)) & 0x55555555;
			return v;
		};
		return spread(pos.x) | (spread(pos.y) << 1);
	}
}
inline uvec2 ZGridToPos(u32 index) {
	if constexpr (PDEP_Available)
		return { intrin::pext_u32(index, 0x55555555),
			     intrin::pext_u32(index, 0xAAAAAAAA) };
	else {
		auto compact = [](u32 v) -> u16 {
			v &= 0x55555555;
			v = (v ^ (v >> 1)) & 0x33333333;
			v = (v ^ (v >> 2)) & 0x0F0F0F0F;
			v = (v ^ (v >> 4)) & 0x00FF00FF;
			v = (v ^ (v >> 8)) & 0x0000FFFF;
			return static_cast<u16>(v);
		};
		return { compact(index), compact(index >> 1) };
	}
} // namespace tx

// ################ PackedPartedView ################

namespace impl {

template <class T>
std::span<T> packedPartedPartition(
    T* dataPtr, u32* metaPtr, u32 partIndex) {
	return std::span<T>(
	    dataPtr + *(metaPtr + partIndex),
	    dataPtr + *(metaPtr + partIndex + 1));
}
} // namespace impl

// deliberately omited bound check
template <class T>
class PackedPartedView {
public:
	PackedPartedView(T* data, u32* meta)
	    : m_data(data), m_meta(meta) {}

	PackedPartedView(const PackedPartedView& other) = default;
	PackedPartedView& operator=(const PackedPartedView& other) = default;
	PackedPartedView(PackedPartedView&& other) = default;
	PackedPartedView& operator=(PackedPartedView&& other) = default;

public:
	using partition = std::span<T>;

	partition at(u32 partIndex) const {
		impl::packedPartedPartition(m_data, m_meta, partIndex);
	}
	partition operator[](u32 partIndex) const { return at(partIndex); }

private:
	T* m_data;
	u32* m_meta;
};



template <std::invocable<std::string_view> Func =
              decltype([](std::string_view str) { std::cout << str << '\n'; })>
inline void checkHardwareSupport(Func&& f = Func{}) {
	if constexpr (!PDEP_Available)
		f("[Warning]: Target machine missing Bit Manipulation Instruction Set 2 "
		  "(BMI2) support. Some features or optimizations might be unavailable.");
}

} // namespace tx