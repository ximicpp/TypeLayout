# Boost.TypeLayout - Compile-Time Benchmarks
#
# These benchmarks measure the compile-time overhead of TypeLayout
# signature generation for types of varying complexity.
#
# Usage:
#   cmake -B build -DCMAKE_CXX_COMPILER=/path/to/p2996-clang++
#   cmake --build build
#
# Or to measure compile time:
#   time cmake --build build --target bench_simple

cmake_minimum_required(VERSION 3.16)
project(TypeLayoutBenchmarks LANGUAGES CXX)

# Find TypeLayout
get_filename_component(TYPELAYOUT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
include_directories(${TYPELAYOUT_ROOT}/include)

# Compiler flags for P2996
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -freflection -freflection-latest -stdlib=libc++")

# Baseline (no TypeLayout instantiations)
add_executable(bench_baseline bench_compile_time.cpp)

# Simple types (5 members)
add_executable(bench_simple bench_compile_time.cpp)
target_compile_definitions(bench_simple PRIVATE BENCH_SIMPLE)

# Medium types (20 members)
add_executable(bench_medium bench_compile_time.cpp)
target_compile_definitions(bench_medium PRIVATE BENCH_MEDIUM)

# Complex types (50+ members)
add_executable(bench_complex bench_compile_time.cpp)
target_compile_definitions(bench_complex PRIVATE BENCH_COMPLEX)

# Very large types (100+ members)
add_executable(bench_very_large bench_compile_time.cpp)
target_compile_definitions(bench_very_large PRIVATE BENCH_VERY_LARGE)

# All benchmarks combined
add_executable(bench_all bench_compile_time.cpp)
target_compile_definitions(bench_all PRIVATE BENCH_ALL)
