/*
 * Copyright 2025 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 */


#include "BenchStats.h"

#include <math.h>
#include <stdlib.h>


void
BenchStats::Reset()
{
	mean = 0.0f;
	median = 0.0f;
	p95 = 0.0f;
	min = 0.0f;
	max = 0.0f;
	stddev = 0.0f;
	cov = 0.0f;
	trimmedMean = 0.0f;
	nSamples = 0;
	nDropped = 0;
	valid = false;
}


static int
_CompareFloat(const void* a, const void* b)
{
	float fa = *(const float*)a;
	float fb = *(const float*)b;
	if (fa < fb) return -1;
	if (fa > fb) return 1;
	return 0;
}


// Linear-interpolation percentile on a sorted array of length n.
// p is in [0, 1]. Matches NumPy default ("linear") so users can
// cross-check our numbers against scripts.
static float
_Percentile(const float* sorted, int32 n, float p)
{
	if (n <= 0)
		return 0.0f;
	if (n == 1)
		return sorted[0];
	if (p <= 0.0f)
		return sorted[0];
	if (p >= 1.0f)
		return sorted[n - 1];

	float idx = p * (float)(n - 1);
	int32 lo = (int32)idx;
	int32 hi = lo + 1;
	if (hi >= n)
		return sorted[n - 1];
	float frac = idx - (float)lo;
	return sorted[lo] + frac * (sorted[hi] - sorted[lo]);
}


BenchStats
BenchStatsCompute::Compute(const float* samples, int32 count)
{
	BenchStats s;
	s.Reset();

	if (samples == NULL || count <= 0)
		return s;

	// Collect valid samples (value >= 0). Preserve a local copy we can sort.
	float valid[kMaxSamples];
	int32 n = 0;
	int32 dropped = 0;
	for (int32 i = 0; i < count && n < kMaxSamples; i++) {
		if (samples[i] >= 0.0f)
			valid[n++] = samples[i];
		else
			dropped++;
	}

	s.nSamples = n;
	s.nDropped = dropped;

	if (n == 0)
		return s;

	// Mean, min, max
	double sum = 0.0;
	float mn = valid[0];
	float mx = valid[0];
	for (int32 i = 0; i < n; i++) {
		sum += valid[i];
		if (valid[i] < mn) mn = valid[i];
		if (valid[i] > mx) mx = valid[i];
	}
	float mean = (float)(sum / (double)n);

	// Stddev (sample, Bessel correction)
	float sd = 0.0f;
	if (n >= 2) {
		double sumSq = 0.0;
		for (int32 i = 0; i < n; i++) {
			double d = (double)valid[i] - (double)mean;
			sumSq += d * d;
		}
		sd = (float)sqrt(sumSq / (double)(n - 1));
	}

	// Sort for percentiles and trimmed mean. Clamp n to the static bound so
	// the compiler can prove memcpy is safe (n is already in [0,kMaxSamples]
	// by construction, but -Wstringop-overflow is happier with an explicit
	// guard).
	int32 nClamped = n;
	if (nClamped > kMaxSamples) nClamped = kMaxSamples;
	float sorted[kMaxSamples];
	for (int32 i = 0; i < nClamped; i++)
		sorted[i] = valid[i];
	qsort(sorted, nClamped, sizeof(float), _CompareFloat);

	float median = _Percentile(sorted, nClamped, 0.50f);
	float p95    = _Percentile(sorted, nClamped, 0.95f);

	// Trimmed mean: drop the single worst outlier using the MAD (median
	// absolute deviation) rule, which is robust against the outlier itself
	// inflating stddev. A point is considered an outlier when
	//   |x - median| > 3 * 1.4826 * MAD
	// (1.4826 is the Gaussian consistency constant). Requires n >= 4 to
	// remain meaningful; otherwise trimmedMean = mean.
	float trimmed = mean;
	if (nClamped >= 4) {
		// Compute MAD
		float absDev[kMaxSamples];
		for (int32 i = 0; i < nClamped; i++)
			absDev[i] = fabsf(valid[i] - median);
		qsort(absDev, nClamped, sizeof(float), _CompareFloat);
		float mad = _Percentile(absDev, nClamped, 0.50f);

		if (mad > 0.0f) {
			float threshold = 3.0f * 1.4826f * mad;
			float worstDelta = 0.0f;
			int32 worstIdx = -1;
			for (int32 i = 0; i < nClamped; i++) {
				float d = fabsf(valid[i] - median);
				if (d > worstDelta) {
					worstDelta = d;
					worstIdx = i;
				}
			}
			if (worstIdx >= 0 && worstDelta > threshold) {
				double ts = 0.0;
				for (int32 i = 0; i < nClamped; i++) {
					if (i != worstIdx)
						ts += valid[i];
				}
				trimmed = (float)(ts / (double)(nClamped - 1));
			}
		}
	}

	s.mean = mean;
	s.median = median;
	s.p95 = p95;
	s.min = mn;
	s.max = mx;
	s.stddev = sd;
	s.cov = (mean > 0.0f) ? sd / mean : 0.0f;
	s.trimmedMean = trimmed;
	s.valid = true;
	return s;
}


float
BenchStatsCompute::CoV(const float* samples, int32 count)
{
	if (samples == NULL || count < 2)
		return 0.0f;

	double sum = 0.0;
	int32 n = 0;
	for (int32 i = 0; i < count; i++) {
		if (samples[i] >= 0.0f) {
			sum += samples[i];
			n++;
		}
	}
	if (n < 2)
		return 0.0f;

	double mean = sum / (double)n;
	if (mean <= 0.0)
		return 0.0f;

	double sumSq = 0.0;
	for (int32 i = 0; i < count; i++) {
		if (samples[i] >= 0.0f) {
			double d = (double)samples[i] - mean;
			sumSq += d * d;
		}
	}
	double sd = sqrt(sumSq / (double)(n - 1));
	return (float)(sd / mean);
}
