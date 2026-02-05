#!/bin/bash
# Boost.TypeLayout - Compile-Time Benchmark Runner
#
# This script measures the compile-time overhead of TypeLayout signature generation
# for types of varying complexity.
#
# Requirements:
#   - Bloomberg Clang P2996 fork
#   - bash with 'time' command
#
# Usage:
#   ./run_benchmarks.sh [compiler_path]
#
# Example:
#   ./run_benchmarks.sh /opt/p2996-clang/bin/clang++

set -e

# Configuration
CXX="${1:-clang++}"
CXXFLAGS="-std=c++26 -freflection -freflection-latest -stdlib=libc++ -I../../include"
SRC="bench_compile_time.cpp"
RUNS=3

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║  Boost.TypeLayout - Compile-Time Benchmark Suite             ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""
echo "Compiler: $CXX"
echo "Flags: $CXXFLAGS"
echo "Runs per benchmark: $RUNS"
echo ""

# Check compiler
if ! command -v "$CXX" &> /dev/null; then
    echo "Error: Compiler not found: $CXX"
    echo "Please provide the path to Bloomberg Clang P2996 fork."
    exit 1
fi

# Baseline (no TypeLayout instantiations)
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Baseline (empty main only):"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
for i in $(seq 1 $RUNS); do
    echo "Run $i:"
    time $CXX $CXXFLAGS $SRC -o /dev/null 2>&1
    echo ""
done

# Simple types (5 members each, 5 types)
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Simple Types (5 members × 5 types = 25 total members):"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
for i in $(seq 1 $RUNS); do
    echo "Run $i:"
    time $CXX $CXXFLAGS -DBENCH_SIMPLE $SRC -o /dev/null 2>&1
    echo ""
done

# Medium types (20 members each, 3 types)
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Medium Types (20 members × 3 types = 60 total members):"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
for i in $(seq 1 $RUNS); do
    echo "Run $i:"
    time $CXX $CXXFLAGS -DBENCH_MEDIUM $SRC -o /dev/null 2>&1
    echo ""
done

# Complex types (30-35 members each, 4 types)
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Complex Types (30-35 members × 4 types = ~130 total members):"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
for i in $(seq 1 $RUNS); do
    echo "Run $i:"
    time $CXX $CXXFLAGS -DBENCH_COMPLEX $SRC -o /dev/null 2>&1
    echo ""
done

# Very large types (40 members × 2 types)
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Very Large Types (40 members × 2 types = 80 total members):"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
for i in $(seq 1 $RUNS); do
    echo "Run $i:"
    time $CXX $CXXFLAGS -DBENCH_VERY_LARGE $SRC -o /dev/null 2>&1
    echo ""
done

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║  Benchmark Complete                                          ║"
echo "╚══════════════════════════════════════════════════════════════╝"
