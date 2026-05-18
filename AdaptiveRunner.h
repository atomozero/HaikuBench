/*
 * Copyright 2025 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef ADAPTIVE_RUNNER_H
#define ADAPTIVE_RUNNER_H


#include <SupportDefs.h>

#include "BenchStats.h"


// Configuration for adaptive sampling.
// Defaults chosen to match the existing SysBenchmark behaviour (3 runs)
// as a floor, while allowing up to 10 runs to converge on noisy tests.
struct AdaptiveConfig {
	int32	minRuns;		// run at least this many times (>= 1)
	int32	maxRuns;		// never run more than this many times
	float	targetCoV;		// stop once CoV drops below this (e.g. 0.02 = 2%)
	bool	dropWorstOfMin;	// if true, runs one extra sample and discards the
							// worst before evaluating CoV. Reduces the impact
							// of a single contaminated first run.

	AdaptiveConfig()
		:
		minRuns(3),
		maxRuns(10),
		targetCoV(0.02f),
		dropWorstOfMin(false)
	{
	}
};


// Function signature for a single benchmark measurement.
// Return a non-negative metric value, or a negative number to signal failure.
typedef float (*SampleFunc)();

// Callback invoked between samples, e.g. to update the UI with
// "run 4 / up to 10". Optional — pass NULL to disable.
// Arguments: (sampleIndex, samplesSoFar, currentCoV, cookie).
typedef void (*ProgressFunc)(int32 sampleIdx, int32 samplesSoFar,
	float currentCoV, void* cookie);


class AdaptiveRunner {
public:
	// Run `fn` adaptively according to `cfg` and return the statistical
	// summary. `progress` is optional.
	//
	// The runner:
	//   1. Always takes `cfg.minRuns` samples first.
	//   2. After each subsequent sample, recomputes CoV and stops as soon
	//      as CoV <= cfg.targetCoV, up to a hard ceiling of cfg.maxRuns.
	//   3. If all samples are invalid, the returned BenchStats has
	//      valid == false.
	static BenchStats Run(SampleFunc fn, const AdaptiveConfig& cfg,
		ProgressFunc progress = NULL, void* cookie = NULL);
};


#endif // ADAPTIVE_RUNNER_H
