/*
 * Copyright 2025 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 *
 * Standalone unit tests for AdaptiveRunner. Build with:
 *   g++ -std=c++14 -O2 -Wall -Wextra \
 *       -o tests/test_adaptive_runner \
 *       tests/test_adaptive_runner.cpp \
 *       AdaptiveRunner.cpp BenchStats.cpp
 */


#include "../AdaptiveRunner.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>


static int sFailures = 0;
static int sChecks = 0;

#define CHECK(cond) do { \
	sChecks++; \
	if (!(cond)) { \
		fprintf(stderr, "FAIL %s:%d : %s\n", __FILE__, __LINE__, #cond); \
		sFailures++; \
	} \
} while (0)


// ---- deterministic fake measurement functions --------------------------

static int sCallCount = 0;

static float fn_constant()        { sCallCount++; return 100.0f; }

// Very stable: CoV < 1% after 2 samples. Runner should stop at minRuns.
static float fn_stable()
{
	static const float seq[] = { 100.0f, 100.5f, 99.7f, 100.2f, 99.9f,
		100.1f, 100.3f, 99.8f, 100.0f, 100.0f };
	float v = seq[sCallCount % (int)(sizeof(seq) / sizeof(seq[0]))];
	sCallCount++;
	return v;
}

// Wildly noisy until the 6th sample, then stabilises around 100.
// Runner must NOT stop at minRuns=3 (CoV too high), should continue
// until it converges.
static float fn_noisy_then_stable()
{
	static const float seq[] = {
		10.0f, 200.0f, 50.0f,    // first 3: huge variance
		100.0f, 100.0f, 100.0f,  // stabilises
		100.0f, 100.0f, 100.0f, 100.0f
	};
	float v = seq[sCallCount % (int)(sizeof(seq) / sizeof(seq[0]))];
	sCallCount++;
	return v;
}

// Always unstable — runner should hit maxRuns.
static float fn_always_noisy()
{
	static const float seq[] = {
		10.0f, 200.0f, 10.0f, 300.0f, 50.0f,
		400.0f, 20.0f, 250.0f, 80.0f, 350.0f
	};
	float v = seq[sCallCount % (int)(sizeof(seq) / sizeof(seq[0]))];
	sCallCount++;
	return v;
}

// All invalid
static float fn_all_invalid() { sCallCount++; return -1.0f; }


// ---- tests ------------------------------------------------------------

static void
test_respects_min_runs()
{
	sCallCount = 0;
	AdaptiveConfig cfg;
	cfg.minRuns = 3;
	cfg.maxRuns = 10;
	cfg.targetCoV = 0.02f;

	BenchStats s = AdaptiveRunner::Run(fn_stable, cfg);
	CHECK(s.valid);
	CHECK(sCallCount >= 3);
	// Stable data: should stop close to minRuns, definitely not hit max
	CHECK(sCallCount <= 5);
}


static void
test_constant_stops_at_min_runs()
{
	// CoV is exactly 0 on constant data; runner should terminate
	// as soon as it has >= 2 valid samples and minRuns is satisfied.
	sCallCount = 0;
	AdaptiveConfig cfg;
	cfg.minRuns = 3;
	cfg.maxRuns = 10;

	BenchStats s = AdaptiveRunner::Run(fn_constant, cfg);
	CHECK(s.valid);
	CHECK(sCallCount == 3);
	CHECK(s.mean == 100.0f);
	CHECK(s.cov == 0.0f);
}


static void
test_continues_on_high_cov_then_converges()
{
	sCallCount = 0;
	AdaptiveConfig cfg;
	cfg.minRuns = 3;
	cfg.maxRuns = 10;
	cfg.targetCoV = 0.05f;

	BenchStats s = AdaptiveRunner::Run(fn_noisy_then_stable, cfg);
	CHECK(s.valid);
	// Must have taken more than the minimum to let the late stable
	// samples pull CoV below the threshold.
	CHECK(sCallCount > 3);
	CHECK(sCallCount <= 10);
	CHECK(s.nSamples == sCallCount);
}


static void
test_caps_at_max_runs()
{
	sCallCount = 0;
	AdaptiveConfig cfg;
	cfg.minRuns = 3;
	cfg.maxRuns = 7;
	cfg.targetCoV = 0.01f; // very tight target

	BenchStats s = AdaptiveRunner::Run(fn_always_noisy, cfg);
	CHECK(s.valid);
	CHECK(sCallCount == 7);
	CHECK(s.cov > cfg.targetCoV); // never converged, as expected
}


static void
test_all_invalid_is_reported()
{
	sCallCount = 0;
	AdaptiveConfig cfg;
	cfg.minRuns = 3;
	cfg.maxRuns = 6;

	BenchStats s = AdaptiveRunner::Run(fn_all_invalid, cfg);
	CHECK(!s.valid);
	CHECK(s.nSamples == 0);
	// Runner should NOT spin forever when everything fails.
	// It should take up to maxRuns and give up.
	CHECK(sCallCount <= cfg.maxRuns);
	CHECK(sCallCount >= cfg.minRuns);
}


static void
test_null_function_is_safe()
{
	AdaptiveConfig cfg;
	BenchStats s = AdaptiveRunner::Run(NULL, cfg);
	CHECK(!s.valid);
}


static void
test_caps_at_kMaxSamples()
{
	// Caller asks for more than kMaxSamples — runner must clamp.
	sCallCount = 0;
	AdaptiveConfig cfg;
	cfg.minRuns = 1;
	cfg.maxRuns = 1000;     // absurd, should be clamped to kMaxSamples
	cfg.targetCoV = 0.0001f; // will never converge on noisy data

	BenchStats s = AdaptiveRunner::Run(fn_always_noisy, cfg);
	CHECK(s.valid);
	CHECK(sCallCount <= kMaxSamples);
}


static int sProgressCalls = 0;
static int sLastSamplesSoFar = 0;
static void
progressCb(int32 /*sampleIdx*/, int32 samplesSoFar, float /*cov*/,
	void* /*cookie*/)
{
	sProgressCalls++;
	sLastSamplesSoFar = samplesSoFar;
}


static void
test_progress_callback()
{
	sCallCount = 0;
	sProgressCalls = 0;
	sLastSamplesSoFar = 0;
	AdaptiveConfig cfg;
	cfg.minRuns = 3;
	cfg.maxRuns = 5;

	BenchStats s = AdaptiveRunner::Run(fn_stable, cfg, progressCb, NULL);
	CHECK(s.valid);
	CHECK(sProgressCalls == sCallCount);
	CHECK(sLastSamplesSoFar == sCallCount);
}


int
main()
{
	test_respects_min_runs();
	test_constant_stops_at_min_runs();
	test_continues_on_high_cov_then_converges();
	test_caps_at_max_runs();
	test_all_invalid_is_reported();
	test_null_function_is_safe();
	test_caps_at_kMaxSamples();
	test_progress_callback();

	printf("AdaptiveRunner: %d checks, %d failures\n", sChecks, sFailures);
	return sFailures == 0 ? 0 : 1;
}
