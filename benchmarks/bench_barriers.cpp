#include "parcomp/barriers/sense_reversing_barrier.hpp"
#include "parcomp/barriers/std_barrier_wrapper.hpp"
#include <barrier>
#include <charconv>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <latch>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

struct BarrierBenchConfig {
    size_t num_threads = 4;
    size_t num_episodes = 200000;
    std::string barrier_type = "all";
};

template <typename BarrierType>
void run_barrier_benchmark(const std::string& name, const BarrierBenchConfig& cfg) {
    BarrierType barrier(cfg.num_threads);
    using Clock = std::chrono::steady_clock;
    Clock::time_point start;
    std::barrier start_gate(static_cast<std::ptrdiff_t>(cfg.num_threads + 1),
                           [&]() noexcept { start = Clock::now(); });
    std::latch launched(1);
    std::latch completed(static_cast<std::ptrdiff_t>(cfg.num_threads));
    bool cancelled = false;
    std::vector<std::thread> threads;
    threads.reserve(cfg.num_threads);

    try {
        for (size_t t = 0; t < cfg.num_threads; ++t) {
            threads.emplace_back([&]() {
                launched.wait();
                if (cancelled) {
                    return;
                }
                for (size_t i = 0; i < 1000; ++i) {
                    barrier.arrive_and_wait();
                }
                start_gate.arrive_and_wait();
                for (size_t i = 0; i < cfg.num_episodes; ++i) {
                    barrier.arrive_and_wait();
                }
                completed.count_down();
            });
        }
    } catch (...) {
        // Release already-created workers without entering an incomplete barrier.
        cancelled = true;
        launched.count_down();
        for (auto& thread : threads) {
            thread.join();
        }
        throw;
    }

    launched.count_down();
    start_gate.arrive_and_wait();
    completed.wait();
    const double elapsed_sec = std::chrono::duration<double>(Clock::now() - start).count();
    for (auto& thread : threads) {
        thread.join();
    }
    if (elapsed_sec <= 0) {
        throw std::runtime_error("Measurement is too short; increase --episodes");
    }
    const double episodes_per_sec = static_cast<double>(cfg.num_episodes) / elapsed_sec;
    std::cout << std::left << std::setw(30) << name
              << " | Threads: " << std::setw(3) << cfg.num_threads
              << " | Episodes: " << std::setw(8) << cfg.num_episodes
              << " | Time: " << std::fixed << std::setprecision(6) << elapsed_sec << " s"
              << " | Rate: " << std::fixed << std::setprecision(2) << episodes_per_sec << " ep/s\n";
}

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n"
              << "  --threads <N>      Positive number of threads (default: 4)\n"
              << "  --episodes <N>     Positive number of measured phases (default: 200000)\n"
              << "  --barrier <type>   sense | sense_backoff | std | all (default: all)\n"
              << "  --help             Display help\n"
              << "Each run includes 1000 untimed warm-up phases.\n";
}

size_t parse_positive(const std::string& value) {
    size_t number = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), number);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size() || number == 0) {
        throw std::invalid_argument("Expected a positive integer: " + value);
    }
    return number;
}

int main(int argc, char** argv) {
    try {
        BarrierBenchConfig cfg;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--help") {
                print_usage(argv[0]);
                return 0;
            }
            if (arg != "--threads" && arg != "--episodes" && arg != "--barrier") {
                throw std::invalid_argument("Unknown option: " + arg);
            }
            if (i + 1 == argc) {
                throw std::invalid_argument("Missing value for " + arg);
            }
            const std::string value = argv[++i];
            if (arg == "--threads") {
                cfg.num_threads = parse_positive(value);
            } else if (arg == "--episodes") {
                cfg.num_episodes = parse_positive(value);
            } else {
                cfg.barrier_type = value;
            }
        }
        if (cfg.barrier_type != "sense" && cfg.barrier_type != "sense_backoff"
            && cfg.barrier_type != "std" && cfg.barrier_type != "all") {
            throw std::invalid_argument("Unknown barrier type: " + cfg.barrier_type);
        }
        if (cfg.num_threads >= static_cast<size_t>(std::barrier<>::max())
            || cfg.num_threads > static_cast<size_t>(std::latch::max())) {
            throw std::invalid_argument("Too many barrier participants");
        }

        std::cout << "========== BARRIER SYNCHRONIZATION PERFORMANCE BENCHMARK ==========\n";
        if (cfg.barrier_type == "sense" || cfg.barrier_type == "all") {
            run_barrier_benchmark<parcomp::SenseReversingBarrier>("SenseReversingBarrier", cfg);
        }
        if (cfg.barrier_type == "sense_backoff" || cfg.barrier_type == "all") {
            struct SenseWithBackoff : parcomp::SenseReversingBarrier {
                explicit SenseWithBackoff(size_t n) : SenseReversingBarrier(n, true) {}
            };
            run_barrier_benchmark<SenseWithBackoff>("SenseReversing (with backoff)", cfg);
        }
        if (cfg.barrier_type == "std" || cfg.barrier_type == "all") {
            run_barrier_benchmark<parcomp::StdBarrierWrapper>("StdBarrierWrapper (baseline)", cfg);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
