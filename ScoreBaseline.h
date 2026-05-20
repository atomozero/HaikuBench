/*
 * Copyright 2025 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 *
 * Reference baseline for the SPEC-style scoring system.
 *
 * Scoring method:
 *   For each test, compute ratio = result / baseline (higher-is-better)
 *   or ratio = baseline / result (lower-is-better, e.g. latency).
 *   Per-category score = geometric mean of ratios * 1000.
 *   Overall score = geometric mean of category scores.
 *
 * A score of 1000 means identical performance to the reference system.
 * Higher is faster, lower is slower.
 *
 * Reference system:
 *   Intel Core i3 M 370 @ 2.40 GHz | 4 cores | 3757 MB RAM
 *   Haiku hrev59722 (R1/beta5+development) x86_64
 *   Measured with adaptive sampling (3-10 runs, CoV < 2%)
 */
#ifndef SCORE_BASELINE_H
#define SCORE_BASELINE_H


#include "SysBenchmark.h"

#include <math.h>


// Reference values — one per SysBenchmark test, same order as
// the fields in SysBenchResults.
static const float kBaseline[SysBenchmark::kNumTests] = {
	// CPU (higher is better: MOPS, MFLOPS, x speedup)
	135.0677f,		// CPU Integer
	19.1699f,		// CPU Float
	2.9019f,		// CPU Multi-thread

	// RAM (higher is better: MB/s; except latency: lower is better)
	5479.9946f,		// RAM Seq Read
	3722.7173f,		// RAM Seq Write
	2752.1802f,		// RAM Copy
	102.2911f,		// RAM Latency (ns — lower is better)

	// Cache (higher is better: MB/s)
	13724.3252f,	// Cache L1
	6871.8799f,		// Cache L2
	5964.5635f,		// Cache L3

	// Kernel (higher is better: kOPS; except syscall overhead: lower)
	387.1967f,		// Sem Create/Delete
	669.6303f,		// Sem Acquire/Release
	113.5000f,		// Sem Contention
	7.2717f,		// Thread Spawn
	346.4428f,		// Port Send/Recv
	99.0802f,		// Area Create/Delete
	46283.5156f,	// Atomic Ops
	47.7637f,		// Syscall Overhead (ns — lower is better)

	// Messaging (higher is better: kOPS)
	312.4707f,		// BMessage Flatten
	62.7608f		// BLooper Messaging
};


// Tests where lower values are better (latency, overhead).
// Index matches kBaseline / SysBenchResults field order.
static inline bool
ScoreIsLowerBetter(int32 testIndex)
{
	return testIndex == 6		// RAM Latency (ns)
		|| testIndex == 17;		// Syscall Overhead (ns)
}


// Category grouping for sub-scores.
enum {
	kScoreCPU = 0,
	kScoreMemory,
	kScoreCache,
	kScoreKernel,
	kScoreMessaging,
	kNumScoreCategories
};

static const char* kScoreCategoryNames[kNumScoreCategories] = {
	"CPU", "Memory", "Cache", "Kernel", "Messaging"
};

// First test index for each category.
static const int32 kCategoryStart[kNumScoreCategories] = {
	0, 3, 7, 10, 18
};

// Number of tests in each category.
static const int32 kCategoryCount[kNumScoreCategories] = {
	3, 4, 3, 8, 2
};


struct ScoreResult {
	float	categoryScore[kNumScoreCategories];
	float	overall;
	bool	valid;
};


static inline ScoreResult
ComputeScore(const SysBenchResults& results)
{
	ScoreResult score;
	score.valid = false;
	score.overall = 0.0f;

	if (!results.valid)
		return score;

	float categoryProduct = 1.0f;
	int32 validCategories = 0;

	for (int32 cat = 0; cat < kNumScoreCategories; cat++) {
		float product = 1.0f;
		int32 validTests = 0;

		for (int32 i = 0; i < kCategoryCount[cat]; i++) {
			int32 idx = kCategoryStart[cat] + i;
			float result = SysBenchmark::ResultValue(results, idx);
			float base = kBaseline[idx];

			if (result <= 0.0f || base <= 0.0f)
				continue;

			float ratio;
			if (ScoreIsLowerBetter(idx))
				ratio = base / result;
			else
				ratio = result / base;

			product *= ratio;
			validTests++;
		}

		if (validTests > 0) {
			score.categoryScore[cat] =
				powf(product, 1.0f / (float)validTests) * 1000.0f;
			categoryProduct *= score.categoryScore[cat];
			validCategories++;
		} else {
			score.categoryScore[cat] = 0.0f;
		}
	}

	if (validCategories > 0) {
		score.overall =
			powf(categoryProduct, 1.0f / (float)validCategories);
		score.valid = true;
	}

	return score;
}


#endif // SCORE_BASELINE_H
