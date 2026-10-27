cmake_minimum_required(VERSION 3.20)
project(BarrierSynchronization VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Threads REQUIRED)
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_LIBRARIES Threads::Threads)
check_cxx_source_compiles("
    #include <barrier>
    #include <latch>
    int main() {
        std::barrier b(1); b.arrive_and_wait();
        std::latch l(1); l.count_down(); l.wait();
    }" HAS_CXX20_BARRIER)
if(NOT HAS_CXX20_BARRIER)
    message(FATAL_ERROR "Compiler and standard library must support C++20 std::barrier and std::latch. See README.md.")
endif()

add_library(parcomp INTERFACE)
target_include_directories(parcomp INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(parcomp INTERFACE Threads::Threads)

foreach(target IN ITEMS test_barriers bench_barriers)
    if(target STREQUAL "test_barriers")
        add_executable(${target} tests/test_barriers.cpp)
    else()
        add_executable(${target} benchmarks/bench_barriers.cpp)
    endif()
    target_link_libraries(${target} PRIVATE parcomp)
    set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

enable_testing()
add_test(NAME test_barriers COMMAND test_barriers)
set_tests_properties(test_barriers PROPERTIES TIMEOUT 120)

add_test(NAME bench_barriers_reject_zero COMMAND bench_barriers --threads 0)
add_test(NAME bench_barriers_reject_negative COMMAND bench_barriers --episodes -1)
add_test(NAME bench_barriers_reject_overflow COMMAND bench_barriers --threads 18446744073709551616)
add_test(NAME bench_barriers_reject_count_limit COMMAND bench_barriers --threads 9223372036854775807)
add_test(NAME bench_barriers_reject_type COMMAND bench_barriers --barrier unknown)
add_test(NAME bench_barriers_reject_missing COMMAND bench_barriers --episodes)
add_test(NAME bench_barriers_reject_unknown COMMAND bench_barriers --unknown)
set_tests_properties(
    bench_barriers_reject_zero bench_barriers_reject_negative
    bench_barriers_reject_overflow bench_barriers_reject_count_limit
    bench_barriers_reject_type bench_barriers_reject_missing bench_barriers_reject_unknown
    PROPERTIES WILL_FAIL TRUE TIMEOUT 5
)

file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/build-info-$<CONFIG>.txt" CONTENT
"Build: $<CONFIG>
Compiler: ${CMAKE_CXX_COMPILER}
Compiler ID/version: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}
System/architecture: ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}
Generator: ${CMAKE_GENERATOR}
CMake: ${CMAKE_VERSION}
C++ standard: ${CMAKE_CXX_STANDARD}
Common flags: ${CMAKE_CXX_FLAGS}
Release flags: ${CMAKE_CXX_FLAGS_RELEASE}
")
