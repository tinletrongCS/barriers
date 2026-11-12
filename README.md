# Barrier synchronization in C++20

A sense-reversing barrier for threads, with correctness tests and a benchmark against `std::barrier`.

I wrote this for the Parallel Computing course (CO5133) at HCMUT. The assignment was a three-person project on synchronization primitives (locks, barriers and counters). The barrier was my part, and this repository holds only that part.

## What is in here

| Path | What it is |
|---|---|
| `include/parcomp/barriers/sense_reversing_barrier.hpp` | The barrier itself (header-only), with optional exponential backoff |
| `include/parcomp/barriers/std_barrier_wrapper.hpp` | `std::barrier` behind the same interface, used as the baseline |
| `include/parcomp/common/backoff.hpp` | The backoff helper |
| `tests/test_barriers.cpp` | Correctness tests |
| `benchmarks/bench_barriers.cpp` | Benchmark program |
| `scripts/run_benchmarks.py` | Runs the tests, repeats the benchmark and writes the results to CSV |

## How the barrier works

A barrier makes a group of N threads wait until all of them have arrived, then lets them all continue. Each round of "everyone arrives, everyone leaves" is one phase.

The sense-reversing version needs two atomics: a counter of threads that have not arrived yet, and a boolean flag (the "sense") that flips once per phase. This is the core of it, with the backoff branch left out:

```cpp
void arrive_and_wait() {
    const bool my_sense = !sense_.load(std::memory_order_relaxed);
    if (count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        // Last thread in: reset the counter, then flip the flag to release everyone.
        count_.store(total_threads_, std::memory_order_relaxed);
        sense_.store(my_sense, std::memory_order_release);
    } else {
        while (sense_.load(std::memory_order_acquire) != my_sense) { /* spin */ }
    }
}
```

Because the flag flips on every phase, the barrier can be reused straight away. Nobody has to clear it between phases.

Using it:

```cpp
#include "parcomp/barriers/sense_reversing_barrier.hpp"

parcomp::SenseReversingBarrier barrier(4);        // 4 threads
parcomp::SenseReversingBarrier gentle(4, true);   // same, with exponential backoff

// in each of the 4 threads, once per phase:
barrier.arrive_and_wait();
```

Things to know before using it:

- The number of threads is fixed when the barrier is created, and it must be at least 1.
- Every thread has to call `arrive_and_wait()` exactly once per phase. There is no way for a thread to drop out.
- Waiting threads spin. That is fast when the wait is short, and wasteful when it is long or when there are more threads than cores.

## Build and test

You need CMake 3.20 or newer and a compiler that has C++20 `std::barrier` and `std::latch`. I have built it with GCC 13 on Ubuntu (WSL2) and with GCC 15 (MinGW) on Windows.

```bash
cmake -B build -S .
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The build type defaults to Release. The commands above are for Linux and WSL. If you already built on Windows, use a different build directory for WSL.

What the tests check:

- After each barrier, every thread sees the value that every other thread wrote for that round. This runs with 1, 2, 3, 4, 8 and 16 threads for 10,001 rounds (1,001 for the backoff version).
- A thread that arrives early does not get through until the last one arrives.
- Two barrier objects can be used by the same group of threads.
- A barrier for zero threads is rejected.
- The benchmark program rejects bad command-line arguments (7 CTest cases).

The same tests also run against `std::barrier`, which is a check on the tests themselves.

## Benchmark

```bash
./build/bin/bench_barriers --threads 4 --episodes 200000
```

```text
SenseReversingBarrier          | Threads: 4   | Episodes: 200000   | Time: 0.081031 s | Rate: 2468197.28 ep/s
SenseReversing (with backoff)  | Threads: 4   | Episodes: 200000   | Time: 0.083321 s | Rate: 2400349.49 ep/s
StdBarrierWrapper (baseline)   | Threads: 4   | Episodes: 200000   | Time: 3.517702 s | Rate: 56855.30 ep/s
```

Options are `--threads N`, `--episodes N` and `--barrier sense|sense_backoff|std|all`.

How it measures:

- The threads do no work between barrier calls, so the number is the cost of the barrier alone.
- Each run starts with 1,000 warm-up phases that are not timed.
- All threads start together through a start gate. The clock starts when the gate opens and stops when the last thread has finished. Creating and joining threads is not included.
- The result is completed phases ("episodes") per second.

To collect a full set of numbers:

```bash
python3 scripts/run_benchmarks.py --machine my-laptop --physical-cores 10 --power-mode "plugged in, balanced"
```

The script runs CTest first and then repeats every thread count 5 times. It saves the raw runs, a median/min/max summary and details about the machine under `results/`, which is ignored by git. It uses only the Python standard library. Its own tests run with `cd scripts && python3 -m unittest test_run_benchmarks`.

## Results

Numbers are phases per second, median of 5 runs, higher is better. Each configuration used 20M phases for 1 thread, 2M for 2 threads, 200K for 4 to 10 threads and 20K for 16 threads.

**Machine 1** is my laptop: Intel Core i7-13620H (10 cores, 16 threads), Windows 11, GCC 15.2 (MinGW), `-O3`.

| Threads | Sense-reversing | With backoff | `std::barrier` | Sense-reversing vs `std::barrier` |
|---:|---:|---:|---:|---:|
| 1 | 117.6 M | 112.4 M | 23.1 M | 5x |
| 2 | 7.45 M | 7.84 M | 246 K | 30x |
| 4 | 2.50 M | 2.47 M | 56.9 K | 44x |
| 8 | 1.14 M | 1.16 M | 16.4 K | 70x |
| 10 | 916 K | 1.02 M | 15.1 K | 61x |
| 16 | 221 K | 259 K | 9.1 K | 24x |

**Machines 2 and 3** belong to my two teammates. Their logs did not record the hardware or the compiler, and some of machine 2's samples were shorter than 0.1 s, so treat its numbers as rough.

| Machine | Threads | Sense-reversing | With backoff | `std::barrier` | Sense-reversing vs `std::barrier` |
|---|---:|---:|---:|---:|---:|
| 2 | 1 | 220.7 M | 222.5 M | 49.5 M | 4.5x |
| 2 | 2 | 12.98 M | 171 K | 8.94 M | 1.5x |
| 2 | 4 | 7.11 M | 99.0 K | 4.04 M | 1.8x |
| 2 | 8 | 1.93 M | 63.4 K | 71.3 K | 27x |
| 3 | 1 | 175.7 M | 176.9 M | 65.6 M | 2.7x |
| 3 | 2 | 8.68 M | 31.7 K | 577 K | 15x |
| 3 | 4 | 3.87 M | 20.7 K | 35.9 K | 108x |
| 3 | 8 | 1.65 M | 16.4 K | 16.3 K | 101x |
| 3 | 16 | 582 K | 13.5 K | 8.2 K | 71x |

### What I take from this

**Spinning wins when there is nothing to do between phases.** With 8 threads the sense-reversing barrier completed between 27 and about 100 times more phases per second than `std::barrier` on the three machines. `std::barrier` blocks the waiting threads and wakes them up through the operating system, and that round trip costs far more than the wait itself, which is around a microsecond or less here.

**That does not make it the better barrier in general.** A spinning thread keeps a core busy while it waits. With 16 threads on a 10-core machine the rate drops to 221 K. In a real program the work done between phases would also hide most of this difference.

**Sleep-based backoff depends heavily on the platform.** The backoff here calls `std::this_thread::sleep_for`, starting at 10 ns and doubling up to 10 µs. On machine 1 under Windows it made no difference. On machines 2 and 3 it made the barrier 30 to 270 times slower than not backing off at all. I see the same on my own laptop when I run the Linux build under WSL2 (GCC 13.3, single run):

| Threads | Sense-reversing | With backoff | `std::barrier` |
|---:|---:|---:|---:|
| 2 | 5.62 M | 25.1 K | 426 K |
| 4 | 1.80 M | 18.8 K | 21.2 K |

On Linux a thread that asks to sleep for 10 ns is really put to sleep for tens of microseconds, which is much longer than the wait it was trying to shorten. On Windows with MinGW the same call returns almost immediately, so the two variants look alike. A backoff that spins for a while instead of sleeping would avoid this. I have not written that version.
