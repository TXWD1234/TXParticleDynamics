// Copyright (c) 2026 TXParticleDynamics. Licensed under the MIT License.

#pragma once
#include "impl/allocator.hpp"
#include "impl/basic_storage.hpp"
#include "impl/data_utils.hpp"
#include "impl/geometry.hpp"
#include "tx/exception.hpp"
#include "tx/type_traits.hpp"
#include <concepts>
#include <atomic>
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
template <class T, tx::allocator Allocator = std::allocator<T>>
class SPSCQueue {
	/**
	 * Assumptions this data structures make:
	 * - push() is always called by a single thread (producer thread)
	 * - pull() / front() / pop() together are always called by a single thread
	 *   (consumer thread)
	 */

	/**
	 * Conventions this data structure expects user to follow:
	 * - front() is always called before pop(). Ideally the user should just
	 *   use pull().
	 * - The queue must be destroyed after the threads are joined. Implying the
	 *   queue to be declared before the thread in the parent class.
	 */

	/**
	 * For implementation details, the established convention is:
	 * - Producer thread owns m_state.end
	 * - Consumer thread owns m_state.begin
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
	~SPSCQueue() { clear(); }

public:
	// ================ Public Interface ================

	template <class... Args>
	    requires std::constructible_from<T, Args...>
	void push(Args&&... args) {
		while (true) {
			u32 old = m_state.begin.load(std::memory_order_acquire);
			if (m_state.end.load(std::memory_order_relaxed) - old < m_storage.size) break;
			m_state.begin.wait(old, std::memory_order_acquire);
		}
		std::construct_at(end_impl(), std::forward<Args>(args)...);
		push_impl();
	}

	T pull() {
		T val = std::move(front());
		pop();
		return val;
	}

	template <class Self>
	tx::const_propagate<Self, T>& front(this Self&& self) {
		while (true) {
			u32 old = self.m_state.end.load(std::memory_order_acquire);
			if (old - self.m_state.begin.load(std::memory_order_relaxed) != 0) break;
			self.m_state.end.wait(old, std::memory_order_acquire);
		}
		return *self.begin_impl();
	}
	void pop() {
		std::destroy_at(begin_impl());
		pop_impl();
	}

	// must be called after all threads are joined
	void clear() {
		while (!producer_empty_impl()) pop();
	}

private:
	tx::BasicStorage<T, Allocator> m_storage;

private:
	// ================ Ring Buffer Implementation ================
	// Re-implementing ring buffer here because access to the foundational
	// indices are required.

	struct {
		alignas(CacheLineSize) std::atomic<u32> begin = 0;
		alignas(CacheLineSize) std::atomic<u32> end = 0;
	} m_state;

	void push_impl() {
		m_state.end.store(
		    m_state.end.load(std::memory_order_relaxed) + 1,
		    std::memory_order_release);
		m_state.end.notify_one();
	}
	void pop_impl() {
		m_state.begin.store(
		    m_state.begin.load(std::memory_order_relaxed) + 1,
		    std::memory_order_release);
		m_state.begin.notify_one();
	}

	template <class Self>
	tx::const_propagate<Self, T>* begin_impl(this Self&& self) {
		return self.m_storage.data +
		       impl::findPowTwoWrappedPhysIndex(
		           self.m_state.begin.load(std::memory_order_relaxed),
		           self.m_storage.size);
	}
	T* end_impl() {
		return m_storage.data +
		       impl::findPowTwoWrappedPhysIndex(
		           m_state.end.load(std::memory_order_relaxed),
		           m_storage.size);
	}

	bool producer_empty_impl() {
		return m_state.end.load(std::memory_order_relaxed) -
		           m_state.begin.load(std::memory_order_acquire) ==
		       0;
	}
};

template <class Argument, class... TArgs>
    requires(
        std::movable<Argument> && std::is_default_constructible_v<Argument> &&
        impl::ForwardLastAllocArgRemainder<TArgs...>::apply(
            []<class... Args>(tx::type_list_t<Args...>) {
	            return tx::satisfies_all<
	                decltype([]<class Arg>(std::type_identity<Arg>) {
		                return std::invocable<Arg, Argument>;
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
	    impl::ForwardLastAllocArgRemainder<TArgs...>, tx::Nothing>::
	    template apply_t<std::variant>;
	struct Task {
		TaskFunc f;
		Argument arg;
	};
	using Allocator = impl::ParseLastAllocArg<
	    std::allocator<Task>, TArgs...>;

private:
	// ================ Worker Function ================

	// @return continue or not
	bool executeTask_impl(Task task) {
		return std::visit(
		    [&](auto& f) {
			    if constexpr (
			        std::same_as<std::remove_cvref_t<decltype(f)>,
			                     tx::Nothing>)
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
	    : m_queue(queueCapacity, alloc),
	      m_thread([this] { worker_impl(); }) {}

	/**
	 * Shutdown behavior: Finish executing all the queued tasks and then
	 * shutdown
	 */
	~TaskThread() {
		m_queue.push(Task{ TaskFunc{ tx::Nothing{} }, Argument{} });
		if (m_thread.joinable()) m_thread.join();
	}

public:
	// ================ Public Interface ================

	// can only be called in one thread (producer thread)
	template <class Func>
	    requires(impl::ForwardLastAllocArgRemainder<TArgs...>::apply(
	        []<class... Args>(tx::type_list_t<Args...>) {
		        return tx::satisfies_any<
		            decltype([]<class Arg>(std::type_identity<Arg>) {
			            return std::same_as<Arg, Func>;
		            }),
		            Args...>;
	        }))
	void assign(Argument arg) {
		m_queue.push(Task{ Func{}, std::move(arg) });
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
#include <immintrin.h>
#else
namespace tx {
constexpr bool PDEP_Available = false;
}
#endif

namespace tx {
inline u32 ZGridToIndex(uvec2 pos) {
	if constexpr (PDEP_Available)
		return _pdep_u32((u16)pos.x, 0x55555555) |
		       _pdep_u32((u16)pos.y, 0xAAAAAAAA);
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
		return {
			static_cast<u16>(_pext_u32((u16)index, 0x55555555)),
			static_cast<u16>(_pext_u32((u16)index, 0xAAAAAAAA))
		};
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
}




template <std::invocable<std::string_view> Func =
              decltype([](std::string_view str) { std::cout << str << '\n'; })>
inline void checkHardwareSupport(Func&& f = Func{}) {
	if constexpr (!PDEP_Available)
		f("[Warning]: Target machine missing Bit Manipulation Instruction Set 2 "
		  "(BMI2) support. Some features or optimizations might be unavailable.");
}

} // namespace tx