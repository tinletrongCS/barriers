#pragma once

#include <cstddef>

namespace parcomp {

/**
 * @brief Base interface for thread barrier synchronization.
 * Reference: Section 2 (Barrier Synchronization) of Assignment 1.
 */
class IBarrier {
public:
    virtual ~IBarrier() = default;

    /**
     * @brief Blocks calling thread until all participating threads have arrived.
     */
    virtual void arrive_and_wait() = 0;
};

} // namespace parcomp
