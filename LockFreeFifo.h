//
// Created by Han on 2024/2/25.
//

#ifndef OPENMP_LOCKFREEFIFO_H
#define OPENMP_LOCKFREEFIFO_H

#include <atomic>
#include <cassert>
#include <memory>
#include <new>

#include <sanitizer/tsan_interface.h>


/// Threadsafe, efficient circular FIFO with cached cursors
template<typename T, typename Alloc = std::allocator<T>>
class Fifo4 : private Alloc
{
public:
    using value_type = T;
    using allocator_traits = std::allocator_traits<Alloc>;
    using size_type = typename allocator_traits::size_type;
    using CursorType = std::atomic<size_type>;

    explicit Fifo4(size_type capacity, Alloc const& alloc = Alloc{})
            : Alloc{alloc}
            , capacity_{capacity}
            , ring_{allocator_traits::allocate(*this, capacity)}
    {}

    ~Fifo4() {
        while(not empty()) {
            element(popCursor_)->~T();
            ++popCursor_;
        }
        allocator_traits::deallocate(*this, ring_, capacity_);
    }

    /// Returns the number of elements in the fifo
    size_type size() const noexcept {
        auto pushCursor = pushCursor_.load(std::memory_order_relaxed);
        auto popCursor = popCursor_.load(std::memory_order_relaxed);

        assert(popCursor <= pushCursor);
        return pushCursor - popCursor;
    }

    /// Returns whether the container has no elements
    bool empty() const noexcept { return size() == 0; }

    /// Returns whether the container has capacity_() elements
    bool full() const noexcept { return size() == capacity(); }

    /// Returns the number of elements that can be held in the fifo
    size_type capacity() const noexcept { return capacity_; }


    /// Push one object onto the fifo.
    /// @return `true` if the operation is successful; `false` if fifo is full.
    bool push(T const& value) {
        auto pushCursor = pushCursor_.load(std::memory_order_relaxed);
        if (full(pushCursor, popCursorCached_)) {
            popCursorCached_ = popCursor_.load(std::memory_order_acquire);
            // popCursorCached_ = popCursor_.load(std::memory_order_relaxed);
            if (full(pushCursor, popCursorCached_)) {
                return false;
            }
        }

        //__tsan_acquire(&popCursor_);
        // std::atomic_thread_fence(std::memory_order_acquire);
        new (element(pushCursor)) T(value);
        pushCursor_.store(pushCursor + 1, std::memory_order_release);
        return true;
    }

    /// Pop one object from the fifo.
    /// @return `true` if the pop operation is successful; `false` if fifo is empty.
    bool pop(T& value) {
        auto popCursor = popCursor_.load(std::memory_order_relaxed);
        if (empty(pushCursorCached_, popCursor)) {
            pushCursorCached_ = pushCursor_.load(std::memory_order_acquire);
            // pushCursorCached_ = pushCursor_.load(std::memory_order_relaxed);
            if (empty(pushCursorCached_, popCursor)) {
                return false;
            }
        }

        // __tsan_acquire(&pushCursor_);
        // std::atomic_thread_fence(std::memory_order_acquire);
        value = *element(popCursor);
        element(popCursor)->~T();
        popCursor_.store(popCursor + 1, std::memory_order_release);
        return true;
    }

private:
    bool full(size_type pushCursor, size_type popCursor) const noexcept {
        return (pushCursor - popCursor) == capacity_;
    }
    static bool empty(size_type pushCursor, size_type popCursor) noexcept {
        return pushCursor == popCursor;
    }
    T* element(size_type cursor) noexcept {
        return &ring_[cursor % capacity_];
    }

private:
    size_type capacity_;
    T* ring_;

    //static_assert(CursorType::is_always_lock_free);

    // See Fifo3 for reason std::hardware_destructive_interference_size is not used directly
    static constexpr auto hardware_destructive_interference_size = size_type{64};

    /// Loaded and stored by the push thread; loaded by the pop thread
    alignas(hardware_destructive_interference_size) CursorType pushCursor_;

    /// Exclusive to the push thread
    alignas(hardware_destructive_interference_size) size_type popCursorCached_{};

    /// Loaded and stored by the pop thread; loaded by the push thread
    alignas(hardware_destructive_interference_size) CursorType popCursor_;

    /// Exclusive to the pop thread
    alignas(hardware_destructive_interference_size) size_type pushCursorCached_{};

    // Padding to avoid false sharing with adjacent objects
    char padding_[hardware_destructive_interference_size - sizeof(size_type)];
};

#endif //OPENMP_LOCKFREEFIFO_H
