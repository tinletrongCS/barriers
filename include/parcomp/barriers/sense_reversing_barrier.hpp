#pragma once

#include "barrier_interface.hpp"
#include "../common/backoff.hpp"
#include <atomic>
#include <stdexcept>

namespace parcomp {

/**
 * @brief Sense-Reversing Barrier
 *
 * Implements centralized sense-reversing barrier for multi-threaded synchronization.
 * Uses an atomic arrival counter and an alternating sense flag.
 * Requires a fixed group of N > 0 participants, each arriving once per phase.
 * Reference: Section 2 (Barrier Synchronization) of Assignment 1.
 */
class SenseReversingBarrier : public IBarrier {
private:
    const size_t total_threads_;
    std::atomic<size_t> count_;
    std::atomic<bool> sense_;
    bool use_backoff_;
public:
    explicit SenseReversingBarrier(size_t num_threads, bool use_backoff = false)
        : total_threads_(num_threads),
          count_(num_threads),
          sense_(false),
          use_backoff_(use_backoff) {
        if (num_threads == 0) {
            throw std::invalid_argument("Cần ít nhất 1 luồng tham gia");
        }
    }

    void arrive_and_wait() override {
        const bool my_sense = !sense_.load(std::memory_order_relaxed);
        const size_t position = count_.fetch_sub(1, std::memory_order_acq_rel);
        if (position == 1)
        {
            count_.store(total_threads_, std::memory_order_relaxed);
            // Đảo giá trị cờ mỗi khi có đủ N luồng đến:sang pha kế tiếp
            sense_.store(my_sense, std::memory_order_release);
        }
        else {
            // nếu có truyền tham số dùng thì mới dùng
            if (use_backoff_)
            {
                ExponentialBackoff backoff;
                while (sense_.load(std::memory_order_acquire) != my_sense) backoff.backoff();
            }
            else
            {
                while (sense_.load(std::memory_order_acquire) != my_sense) {}
            }
        }
    }
};

} // namespace parcomp
