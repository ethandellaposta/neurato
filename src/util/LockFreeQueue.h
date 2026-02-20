#pragma once

#include <atomic>
#include <array>
#include <cstddef>
#include <optional>

namespace neurato {

// Single-Producer Single-Consumer lock-free FIFO queue.
// RT-safe: no allocations, no locks, no syscalls.
// Uses a fixed-size ring buffer with atomic indices.
template <typename T, size_t Capacity>
class LockFreeQueue
{
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");

public:
    LockFreeQueue() : writeIndex_(0), readIndex_(0) {}

    // Called from producer thread only. Returns false if full.
    bool tryPush(const T& item) noexcept
    {
        const size_t w = writeIndex_.load(std::memory_order_relaxed);
        const size_t nextW = (w + 1) & mask_;
        if (nextW == readIndex_.load(std::memory_order_acquire))
            return false; // full
        buffer_[w] = item;
        writeIndex_.store(nextW, std::memory_order_release);
        return true;
    }

    // Called from consumer thread only. Returns std::nullopt if empty.
    std::optional<T> tryPop() noexcept
    {
        const size_t r = readIndex_.load(std::memory_order_relaxed);
        if (r == writeIndex_.load(std::memory_order_acquire))
            return std::nullopt; // empty
        T item = buffer_[r];
        readIndex_.store((r + 1) & mask_, std::memory_order_release);
        return item;
    }

    bool isEmpty() const noexcept
    {
        return readIndex_.load(std::memory_order_acquire) ==
               writeIndex_.load(std::memory_order_acquire);
    }

    size_t size() const noexcept
    {
        const size_t w = writeIndex_.load(std::memory_order_acquire);
        const size_t r = readIndex_.load(std::memory_order_acquire);
        return (w - r) & mask_;
    }

private:
    static constexpr size_t mask_ = Capacity - 1;
    std::array<T, Capacity> buffer_{};
    alignas(64) std::atomic<size_t> writeIndex_;
    alignas(64) std::atomic<size_t> readIndex_;
};

} // namespace neurato
