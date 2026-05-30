#pragma once
#include <array>
#include <atomic>
#include <cstddef>

namespace nano_exchange
{
    // Lock-free Single-Producer Single-Consumer queue.
    // Capacity must be a power of 2.
    // Producer owns write_idx_; consumer owns read_idx_.
    // alignas(64) puts each index on its own cache line, preventing false sharing.
    template<typename T, size_t Capacity>
    class SPSCQueue
    {
        static_assert((Capacity & (Capacity - 1)) == 0,
                      "Capacity must be a power of 2");

    private:
        alignas(64) std::atomic<size_t> write_idx_{0};
        alignas(64) std::atomic<size_t> read_idx_{0};
        std::array<T, Capacity> buffer_;

    public:
        // Returns false if the queue is full (caller must retry).
        bool push(const T& item) noexcept
        {
            const size_t w    = write_idx_.load(std::memory_order_relaxed);
            const size_t next = (w + 1) & (Capacity - 1);

            if (next == read_idx_.load(std::memory_order_acquire))
                return false;

            buffer_[w] = item;
            write_idx_.store(next, std::memory_order_release);
            return true;
        }

        // Returns false if the queue is empty.
        bool pop(T& item) noexcept
        {
            const size_t r = read_idx_.load(std::memory_order_relaxed);

            if (r == write_idx_.load(std::memory_order_acquire))
                return false;

            item = buffer_[r];
            read_idx_.store((r + 1) & (Capacity - 1), std::memory_order_release);
            return true;
        }

        bool empty() const noexcept
        {
            return read_idx_.load(std::memory_order_acquire) ==
                   write_idx_.load(std::memory_order_acquire);
        }
    };
}
