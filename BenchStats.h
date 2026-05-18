/*
 * Copyright 2025 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef BENCH_STATS_H
#define BENCH_STATS_H


#include <SupportDefs.h>


// Maximum number of samples we are willing to keep per test.
// Adaptive runners bail out well before this in practice.
static const int32 kMaxSamples = 32;


// Statistical summary of a set of benchmark samples.
//
// All fields are populated by BenchStats::Compute(). Values with
// nSamples == 0 or all-invalid input are reported as 0.0 and valid=false
// so callers can detect "no measurement" without sentinel magic numbers.
struct BenchStats {
	float	mean;		// arithmetic mean of valid samples
	float	median;		// p50
	float	p95;		// 95th percentile (linear interpolation)
	float	min;
	float	max;
	float	stddev;		// sample standard deviation (Bessel n-1)
	float	cov;		// coefficient of variation = stddev / mean
	float	trimmedMean;// mean after discarding the worst outlier detected
	                    // via MAD rule (median absolute deviation, 3*1.4826)
	int32	nSamples;	// number of valid samples used
	int32	nDropped;	// samples rejected as invalid (< 0)
	bool	valid;		// true if at least one valid sample was seen

	void Reset();
};


// Pure, side-effect-free statistics helpers.
// Kept as a namespace-like class so we can mock / unit test without
// pulling in the rest of the benchmark machinery.
class BenchStatsCompute {
public:
	// Compute the full statistical summary from `samples[0..count)`.
	// Samples with value < 0.0f are treated as invalid and skipped
	// (matching the existing SysBenchmark convention).
	static BenchStats Compute(const float* samples, int32 count);

	// Coefficient of variation of the current valid samples.
	// Useful for adaptive stopping without computing the whole summary.
	// Returns 0.0 if fewer than 2 valid samples exist.
	static float CoV(const float* samples, int32 count);
};


#endif // BENCH_STATS_H
