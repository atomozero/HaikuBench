/*
 * Copyright 2025 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef BENCH_WARMUP_H
#define BENCH_WARMUP_H


#include <OS.h>


// Warm-up helpers. The goal is to reach a reproducible steady state
// before the actual measurement: CPU frequency governor at peak,
// working set pulled into cache, TLB populated, JIT / relocations
// resolved, malloc arenas warmed, etc.
//
// Usage pattern:
//   BenchWarmup::SpinMs(200);   // generic: drive CPU up
//   // optionally: call the tested function once to touch its buffers
//   real_measurement();
//
// The warm-up routines are intentionally tiny and header-light so we
// can unit-test them without pulling in the rest of HaikuBench.
class BenchWarmup {
public:
	// Generic CPU-side warm-up: spin in user space for `durationMs`
	// milliseconds. Uses system_time() for timing and a volatile
	// accumulator to prevent the compiler from optimising the loop away.
	// Returns the actual elapsed time in microseconds.
	static bigtime_t	SpinMs(int32 durationMs);

	// Buffer-touch warm-up: write one byte per page across `size` bytes
	// of `buffer`. Ensures pages are mapped and hot in TLB / L1.
	// Safe with NULL / zero size (no-op).
	static void			TouchBuffer(void* buffer, size_t size);

	// Default warm-up duration used by SysBenchmark before each test.
	// Exposed so tests and callers stay in sync.
	static const int32	kDefaultWarmupMs = 200;
};


#endif // BENCH_WARMUP_H
