#pragma once

#include <chrono>
#include <thread>
#include <algorithm>
#include <random>

namespace parcomp {

/**
 * @brief Exponential Backoff helper as described in Assignment 1 (page 2).
 *
 * When contention occurs (e.g. CAS failure or spinlock contention), threads wait
 * for an exponentially increasing delay before retrying.
 */
class ExponentialBackoff {
public:
    explicit ExponentialBackoff(unsigned min_delay_ns = 10, unsigned max_delay_ns = 10000)
        : min_delay_(min_delay_ns), max_delay_(max_delay_ns), current_delay_(min_delay_ns) {}

    /**
     * @brief Pause/sleep for the current delay duration and double the delay
     * up to max_delay.
     *
     * // TODO: [Assignment 1 - Section 1, 2, 3]
     * You can customize the backoff strategy here:
     * - std::this_thread::sleep_for(std::chrono::nanoseconds(current_delay_))
     * - or busy-wait cycles / CPU pause instructions for ultra-low latency spinlocks
     * - or randomized jitter backoff
     */
    void backoff() {
        // Default sleep-based backoff as shown in assignment code fragment:
        std::this_thread::sleep_for(std::chrono::nanoseconds(current_delay_));
        if (current_delay_ < max_delay_) {
            current_delay_ = std::min(current_delay_ * 2, max_delay_);
        }
    }

    /**
     * @brief Reset delay back to min_delay after acquiring lock or succeeding CAS.
     */
    void reset() {
        current_delay_ = min_delay_;
    }

    [[nodiscard]] unsigned current_delay() const { return current_delay_; }
    [[nodiscard]] unsigned max_delay() const { return max_delay_; }

private:
    unsigned min_delay_;
    unsigned max_delay_;
    unsigned current_delay_;
};

} // namespace parcomp
