/*
 * Copyright 2025 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 *
 * Integration smoke test for Phase A. Exercises the full pipeline
 * (warm-up -> adaptive runner -> BenchStats) against a deterministic
 * synthetic measurement, validating the invariants that SysBenchmark
 * now relies on. Intentionally avoids running the real 20 tests
 * (which take minutes) — it targets the glue, not the workload.
 *
 * Build:
 *   g++ -std=c++14 -O2 -Wall -Wextra \
 *       -o tests/test_phase_a_integration \
 *       tests/test_phase_a_integration.cpp \
 *       AdaptiveRunner.cpp BenchStats.cpp BenchWarmup.cpp -lbe
 */


#include "../AdaptiveRunner.h"
#include "../BenchStats.h"
#include "../BenchWarmup.h"

#include <OS.h>

#include <math.h>
#include <stdio.h>


static int sFailures = 0;
static int sChecks = 0;

#define CHECK(cond) do { \
	sChecks++; \
	if (!(cond)) { \
		fprintf(stderr, "FAIL %s:%d : %s\n", __FILE__, __LINE__, #cond); \
		sFailures++; \
	} \
} while (0)


// A tiny but realistic "CPU integer" workload. Busy-loops for ~50 ms
// of wall time doing integer arithmetic, returns a throughput number.
// Output is stable enough that the adaptive runner should converge in
// a handful of samples.
static float
fake_cpu_work()
{
	volatile int64 a = 1, b = 2, c = 3;
	int64 ops = 0;
	bigtime_t start = system_time();
	bigtime_t deadline = start + 50000; // 50 ms

	while (system_time() < deadline) {
		for (int32 i = 0; i < 10000; i++) {
			a = a + b * c;
			b = (a ^ c) + (b >> 1);
			c = (a + b) / ((c & 0x7F) | 1);
		}
		ops += 30000;
	}
	bigtime_t elapsed = system_time() - start;
	if (elapsed < 1000)
		return -1.0f;
	return (float)ops / (float)elapsed;
}


static void
test_pipeline_produces_valid_stats()
{
	// Full pipeline: warm up, then let the adaptive runner drive
	// the work function.
	BenchWarmup::SpinMs(50);

	AdaptiveConfig cfg;
	cfg.minRuns = 3;
	cfg.maxRuns = 8;
	cfg.targetCoV = 0.05f;

	BenchStats s = AdaptiveRunner::Run(fake_cpu_work, cfg);

	CHECK(s.valid);
	CHECK(s.nSamples >= 3);
	CHECK(s.nSamples <= 8);
	CHECK(s.nDropped == 0);

	// Throughput must be strictly positive.
	CHECK(s.mean > 0.0f);
	CHECK(s.median > 0.0f);
	CHECK(s.p95 > 0.0f);
	CHECK(s.min > 0.0f);
	CHECK(s.max > 0.0f);

	// Ordering invariants the downstream consumers rely on.
	CHECK(s.min <= s.median);
	CHECK(s.median <= s.p95);
	CHECK(s.p95 <= s.max);
	CHECK(s.stddev >= 0.0f);
	CHECK(s.cov >= 0.0f);

	// On a reasonable system this converges below ~30% CoV easily.
	// We use a loose bound so the test is not flaky on contended CI.
	CHECK(s.cov < 0.30f);

	// Trimmed mean must be in [min, max]
	CHECK(s.trimmedMean >= s.min);
	CHECK(s.trimmedMean <= s.max);
}


static void
test_progress_sees_growing_counts()
{
	struct Ctx { int32 lastCount; int32 seen; } ctx = { 0, 0 };
	auto cb = [](int32 /*idx*/, int32 samplesSoFar, float /*cov*/,
			void* cookie) {
		Ctx* c = (Ctx*)cookie;
		if (samplesSoFar < c->lastCount) {
			// Must never go backwards
			fprintf(stderr, "progress went backwards: %d < %d\n",
				(int)samplesSoFar, (int)c->lastCount);
		}
		c->lastCount = samplesSoFar;
		c->seen++;
	};

	AdaptiveConfig cfg;
	cfg.minRuns = 3;
	cfg.maxRuns = 5;

	BenchStats s = AdaptiveRunner::Run(fake_cpu_work, cfg, cb, &ctx);
	CHECK(s.valid);
	CHECK(ctx.seen == s.nSamples);
	CHECK(ctx.lastCount == s.nSamples);
}


int
main()
{
	test_pipeline_produces_valid_stats();
	test_progress_sees_growing_counts();

	printf("Phase A integration: %d checks, %d failures\n",
		sChecks, sFailures);
	return sFailures == 0 ? 0 : 1;
}
