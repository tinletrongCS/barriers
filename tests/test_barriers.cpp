#include "parcomp/barriers/sense_reversing_barrier.hpp"
#include "parcomp/barriers/std_barrier_wrapper.hpp"
#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

void check(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename BarrierType>
void test_phases(const std::string& name, size_t num_threads, size_t num_rounds,
                 bool two_instances = false) {
    std::cout << "[TEST] " << name << ": " << num_threads << " threads, "
              << num_rounds << " rounds, " << (two_instances ? 2 : 1)
              << " barrier(s)..." << std::flush;

    BarrierType barrier(num_threads);
    BarrierType second(num_threads);
    parcomp::IBarrier& finish = two_instances ? second : barrier;
    std::vector<int> phase_data(num_threads, -1);
    std::atomic<bool> failed{false};
    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (size_t round = 0; round < num_rounds; ++round) {
                if (t + 1 == num_threads && round % 127 == 0) {
                    std::this_thread::yield();
                }
                phase_data[t] = static_cast<int>(round);
                barrier.arrive_and_wait();
                for (size_t i = 0; i < num_threads; ++i) {
                    if (phase_data[i] != static_cast<int>(round)) {
                        failed.store(true, std::memory_order_relaxed);
                    }
                }
                // Prevent next-round writes while peers still read this round.
                finish.arrive_and_wait();
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    check(!failed.load(), name + ": phase data mismatch");
    std::cout << " PASSED!\n";
}

template <typename BarrierType>
void test_delayed_arrival(const std::string& name) {
    std::cout << "[TEST] " << name << ": second participant arrives late..."
              << std::flush;
    BarrierType first(1);
    BarrierType second(2);
    std::promise<void> entering;
    std::promise<void> returned;
    auto entering_future = entering.get_future();
    auto returned_future = returned.get_future();
    std::thread worker([&]() {
        first.arrive_and_wait();
        entering.set_value();
        second.arrive_and_wait();
        returned.set_value();
    });

    entering_future.wait();
    const bool early_return = returned_future.wait_for(std::chrono::milliseconds(50))
                              == std::future_status::ready;
    second.arrive_and_wait();
    worker.join();
    check(!early_return, name + ": returned before the second participant arrived");
    std::cout << " PASSED!\n";
}

template <typename BarrierType>
void run_suite(const std::string& name, size_t rounds) {
    test_delayed_arrival<BarrierType>(name);
    for (size_t n : {1u, 2u, 3u, 4u, 8u, 16u}) {
        test_phases<BarrierType>(name, n, rounds);
    }
    test_phases<BarrierType>(name, 4, 1001, true);
}

int main() {
    try {
        std::cout << "========== SECTION 2: BARRIER CORRECTNESS TESTS ==========\n";
        bool rejected_zero = false;
        try {
            parcomp::SenseReversingBarrier barrier(0);
        } catch (const std::invalid_argument&) {
            rejected_zero = true;
        }
        check(rejected_zero, "Zero participants must be rejected");
        std::cout << "[TEST] Zero participants... PASSED!\n";

        struct SenseWithBackoff : parcomp::SenseReversingBarrier {
            explicit SenseWithBackoff(size_t n) : SenseReversingBarrier(n, true) {}
        };
        run_suite<parcomp::SenseReversingBarrier>("SenseReversing", 10001);
        run_suite<SenseWithBackoff>("SenseReversing with backoff", 1001);
        run_suite<parcomp::StdBarrierWrapper>("std::barrier", 10001);
        std::cout << "All barrier correctness tests passed successfully!\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
