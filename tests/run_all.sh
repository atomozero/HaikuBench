#!/bin/sh
#
# Copyright 2025 Andrea Bernardi
# All rights reserved. Distributed under the terms of the MIT License.
#
# Build and run every unit/integration test in tests/.
# Exits non-zero at the first failing suite.

set -e
cd "$(dirname "$0")/.."

CXX=${CXX:-g++}
CXXFLAGS="-std=c++14 -O2 -Wall -Wextra"

echo "== Building tests =="
$CXX $CXXFLAGS -o tests/test_bench_stats \
	tests/test_bench_stats.cpp BenchStats.cpp

$CXX $CXXFLAGS -o tests/test_adaptive_runner \
	tests/test_adaptive_runner.cpp AdaptiveRunner.cpp BenchStats.cpp

$CXX $CXXFLAGS -o tests/test_bench_warmup \
	tests/test_bench_warmup.cpp BenchWarmup.cpp -lbe

$CXX $CXXFLAGS -o tests/test_phase_a_integration \
	tests/test_phase_a_integration.cpp \
	AdaptiveRunner.cpp BenchStats.cpp BenchWarmup.cpp -lbe

echo
echo "== Running tests =="
./tests/test_bench_stats
./tests/test_adaptive_runner
./tests/test_bench_warmup
./tests/test_phase_a_integration

echo
echo "All tests passed."
