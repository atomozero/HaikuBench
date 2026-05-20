/*
 * Copyright 2025 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 *
 * Standalone unit tests for BenchStats. Build with:
 *   g++ -std=c++14 -O2 -Wall -Wextra -o tests/test_bench_stats \
 *       tests/test_bench_stats.cpp BenchStats.cpp
 * Run with:
 *   ./tests/test_bench_stats
 */


#include "../BenchStats.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>


static int sFailures = 0;
static int sChecks = 0;


static bool
approx(float a, float b, float eps = 1e-4f)
{
	return fabsf(a - b) <= eps * (1.0f + fabsf(b));
}


#define CHECK(cond) do { \
	sChecks++; \
	if (!(cond)) { \
		fprintf(stderr, "FAIL %s:%d : %s\n", __FILE__, __LINE__, #cond); \
		sFailures++; \
	} \
} while (0)

#define CHECK_APPROX(a, b) do { \
	sChecks++; \
	if (!approx((a), (b))) { \
		fprintf(stderr, "FAIL %s:%d : %s (%.6f) !~= %s (%.6f)\n", \
			__FILE__, __LINE__, #a, (double)(a), #b, (double)(b)); \
		sFailures++; \
	} \
} while (0)


static void
test_empty()
{
	BenchStats s = BenchStatsCompute::Compute(NULL, 0);
	CHECK(!s.valid);
	CHECK(s.nSamples == 0);
	CHECK(s.mean == 0.0f);

	float empty[1] = { 0.0f };
	s = BenchStatsCompute::Compute(empty, 0);
	CHECK(!s.valid);
}


static void
test_all_invalid()
{
	float samples[] = { -1.0f, -2.0f, -0.5f };
	BenchStats s = BenchStatsCompute::Compute(samples, 3);
	CHECK(!s.valid);
	CHECK(s.nSamples == 0);
	CHECK(s.nDropped == 3);
}


static void
test_single_sample()
{
	float samples[] = { 42.0f };
	BenchStats s = BenchStatsCompute::Compute(samples, 1);
	CHECK(s.valid);
	CHECK(s.nSamples == 1);
	CHECK_APPROX(s.mean, 42.0f);
	CHECK_APPROX(s.median, 42.0f);
	CHECK_APPROX(s.p95, 42.0f);
	CHECK_APPROX(s.min, 42.0f);
	CHECK_APPROX(s.max, 42.0f);
	CHECK_APPROX(s.stddev, 0.0f);
	CHECK_APPROX(s.cov, 0.0f);
	CHECK_APPROX(s.trimmedMean, 42.0f);
}


static void
test_known_values()
{
	// Dataset: 10, 20, 30, 40, 50
	// mean = 30, median = 30, min=10 max=50
	// variance (sample) = sum((x-30)^2)/4 = (400+100+0+100+400)/4 = 250
	// stddev = sqrt(250) ~= 15.81139
	// CoV = 15.81139 / 30 ~= 0.527046
	// p95: idx = 0.95*4 = 3.8, lo=3 (40), hi=4 (50), frac=0.8 -> 48
	float samples[] = { 10.0f, 20.0f, 30.0f, 40.0f, 50.0f };
	BenchStats s = BenchStatsCompute::Compute(samples, 5);

	CHECK(s.valid);
	CHECK(s.nSamples == 5);
	CHECK(s.nDropped == 0);
	CHECK_APPROX(s.mean, 30.0f);
	CHECK_APPROX(s.median, 30.0f);
	CHECK_APPROX(s.min, 10.0f);
	CHECK_APPROX(s.max, 50.0f);
	CHECK_APPROX(s.stddev, sqrtf(250.0f));
	CHECK_APPROX(s.cov, sqrtf(250.0f) / 30.0f);
	CHECK_APPROX(s.p95, 48.0f);
}


static void
test_percentile_unsorted_input()
{
	// Same values, scrambled. Percentiles must sort first.
	float samples[] = { 50.0f, 10.0f, 40.0f, 20.0f, 30.0f };
	BenchStats s = BenchStatsCompute::Compute(samples, 5);
	CHECK_APPROX(s.median, 30.0f);
	CHECK_APPROX(s.p95, 48.0f);
	CHECK_APPROX(s.min, 10.0f);
	CHECK_APPROX(s.max, 50.0f);
}


static void
test_drops_invalid_but_keeps_valid()
{
	float samples[] = { 10.0f, -1.0f, 20.0f, -2.0f, 30.0f };
	BenchStats s = BenchStatsCompute::Compute(samples, 5);
	CHECK(s.valid);
	CHECK(s.nSamples == 3);
	CHECK(s.nDropped == 2);
	CHECK_APPROX(s.mean, 20.0f);
	CHECK_APPROX(s.median, 20.0f);
}


static void
test_trimmed_mean_drops_outlier()
{
	// 4 tight samples around 100 plus one big outlier.
	// The raw mean is pulled up; the trimmed mean should stay near 100.
	float samples[] = { 100.0f, 101.0f, 99.0f, 100.0f, 500.0f };
	BenchStats s = BenchStatsCompute::Compute(samples, 5);
	CHECK(s.valid);
	CHECK(s.mean > 150.0f);            // raw mean contaminated
	CHECK(s.trimmedMean < 105.0f);     // trimmed mean clean
	CHECK(s.trimmedMean > 95.0f);
}


static void
test_trimmed_mean_preserves_clean_data()
{
	// Tight cluster, no outlier: trimmed == mean.
	float samples[] = { 100.0f, 101.0f, 99.0f, 100.5f, 99.5f };
	BenchStats s = BenchStatsCompute::Compute(samples, 5);
	CHECK_APPROX(s.trimmedMean, s.mean);
}


static void
test_cov_helper()
{
	float samples[] = { 10.0f, 20.0f, 30.0f, 40.0f, 50.0f };
	float cov = BenchStatsCompute::CoV(samples, 5);
	CHECK_APPROX(cov, sqrtf(250.0f) / 30.0f);

	// Constant data -> CoV = 0
	float flat[] = { 5.0f, 5.0f, 5.0f };
	CHECK_APPROX(BenchStatsCompute::CoV(flat, 3), 0.0f);

	// Too few samples -> 0
	float one[] = { 1.0f };
	CHECK_APPROX(BenchStatsCompute::CoV(one, 1), 0.0f);
}


static void
test_cov_ignores_invalid()
{
	float samples[] = { 10.0f, -1.0f, 20.0f, 30.0f };
	float cov = BenchStatsCompute::CoV(samples, 4);
	// Expected over {10, 20, 30}: mean=20, sd=sqrt(((100+0+100)/2))=10, cov=0.5
	CHECK_APPROX(cov, 0.5f);
}


int
main()
{
	test_empty();
	test_all_invalid();
	test_single_sample();
	test_known_values();
	test_percentile_unsorted_input();
	test_drops_invalid_but_keeps_valid();
	test_trimmed_mean_drops_outlier();
	test_trimmed_mean_preserves_clean_data();
	test_cov_helper();
	test_cov_ignores_invalid();

	printf("BenchStats: %d checks, %d failures\n", sChecks, sFailures);
	return sFailures == 0 ? 0 : 1;
}
