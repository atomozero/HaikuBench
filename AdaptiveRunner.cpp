/*
 * Copyright 2025 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 */


#include "AdaptiveRunner.h"


BenchStats
AdaptiveRunner::Run(SampleFunc fn, const AdaptiveConfig& cfg,
	ProgressFunc progress, void* cookie)
{
	BenchStats empty;
	empty.Reset();
	if (fn == NULL)
		return empty;

	// Normalise the configuration. These guards make the runner safe to
	// call with a zero-initialised AdaptiveConfig and keep kMaxSamples
	// as a hard upper bound regardless of what the caller requested.
	int32 minRuns = cfg.minRuns < 1 ? 1 : cfg.minRuns;
	int32 maxRuns = cfg.maxRuns < minRuns ? minRuns : cfg.maxRuns;
	if (maxRuns > kMaxSamples)
		maxRuns = kMaxSamples;
	if (minRuns > maxRuns)
		minRuns = maxRuns;

	float targetCoV = cfg.targetCoV;
	if (targetCoV < 0.0f)
		targetCoV = 0.0f;

	float samples[kMaxSamples];
	int32 n = 0;

	// Phase 1: unconditional minRuns
	for (; n < minRuns; n++) {
		samples[n] = fn();
		float cov = (n >= 1) ? BenchStatsCompute::CoV(samples, n + 1) : 0.0f;
		if (progress != NULL)
			progress(n, n + 1, cov, cookie);
	}

	// Phase 2: adaptive, one sample at a time until CoV converges or we
	// hit the ceiling. CoV is only meaningful with >= 2 valid samples; if
	// every sample so far has been invalid, keep going until maxRuns.
	while (n < maxRuns) {
		float cov = BenchStatsCompute::CoV(samples, n);
		// CoV == 0 with n>=2 means either identical samples or all invalid.
		// We use the stats summary to distinguish: if we already have
		// valid samples and CoV <= target, we are done.
		if (n >= minRuns && cov > 0.0f && cov <= targetCoV)
			break;
		// Special case: perfectly stable measurement (e.g. a synthetic
		// constant-time test) -> CoV truly 0. Stop once at least 2 valid
		// samples agree.
		if (n >= minRuns && cov == 0.0f) {
			BenchStats peek = BenchStatsCompute::Compute(samples, n);
			if (peek.valid && peek.nSamples >= 2)
				break;
		}

		samples[n] = fn();
		n++;
		float newCov = BenchStatsCompute::CoV(samples, n);
		if (progress != NULL)
			progress(n - 1, n, newCov, cookie);
	}

	return BenchStatsCompute::Compute(samples, n);
}
