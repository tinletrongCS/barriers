#pragma once

#include "parcomp/barriers/barrier_interface.hpp"
#include <barrier>

namespace parcomp {

/**
 * @brief Adapter wrapping std::barrier (C++20) to conform to IBarrier.
 * Used as a baseline for performance comparisons in Assignment 1.
 */
class StdBarrierWrapper : public IBarrier {
public:
    explicit StdBarrierWrapper(size_t num_threads)
        : barrier_(static_cast<std::ptrdiff_t>(num_threads)) {}

    void arrive_and_wait() override {
        barrier_.arrive_and_wait();
    }

private:
    std::barrier<> barrier_;
};

} // namespace parcomp
