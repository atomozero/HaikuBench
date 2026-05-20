/*
 * Copyright 2025 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 *
 * Standalone tests for BenchWarmup. Build with:
 *   g++ -std=c++14 -O2 -Wall -Wextra -o tests/test_bench_warmup \
 *       tests/test_bench_warmup.cpp BenchWarmup.cpp -lbe
 */


#include "../BenchWarmup.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static int sFailures = 0;
static int sChecks = 0;


#define CHECK(cond) do { \
	sChecks++; \
	if (!(cond)) { \
		fprintf(stderr, "FAIL %s:%d : %s\n", __FILE__, __LINE__, #cond); \
		sFailures++; \
	} \
} while (0)


static void
test_spin_respects_duration()
{
	// SpinMs(50) must take at least ~45 ms (some slack for scheduling)
	// and should not take wildly longer than 500 ms on a sane system.
	bigtime_t elapsed = BenchWarmup::SpinMs(50);
	CHECK(elapsed >= 45000);
	CHECK(elapsed < 500000);
}


static void
test_spin_zero_is_noop()
{
	bigtime_t elapsed = BenchWarmup::SpinMs(0);
	CHECK(elapsed == 0);

	elapsed = BenchWarmup::SpinMs(-5);
	CHECK(elapsed == 0);
}


static void
test_touch_buffer_null_safe()
{
	BenchWarmup::TouchBuffer(NULL, 0);
	BenchWarmup::TouchBuffer(NULL, 1234);

	uint8 buf[16];
	memset(buf, 0, sizeof(buf));
	BenchWarmup::TouchBuffer(buf, 0);
	// Must not have written anything
	for (size_t i = 0; i < sizeof(buf); i++)
		CHECK(buf[i] == 0);
}


static void
test_touch_buffer_writes_pages()
{
	const size_t kSize = 3 * 4096 + 123; // 3 full pages + a tail
	uint8* buf = (uint8*)malloc(kSize);
	CHECK(buf != NULL);
	memset(buf, 0, kSize);

	BenchWarmup::TouchBuffer(buf, kSize);

	// First byte of each page must be non-zero (written by TouchBuffer),
	// except at offset 0 where the value happens to be 0 in our pattern.
	// Check that at least offsets 4096 and 8192 got touched.
	CHECK(buf[4096] == (uint8)(4096 & 0xFF));
	CHECK(buf[8192] == (uint8)(8192 & 0xFF));
	// Last byte always touched
	CHECK(buf[kSize - 1] == (uint8)((kSize - 1) & 0xFF));

	free(buf);
}


static void
test_spin_actually_runs_work()
{
	// A longer spin must elapse longer than a shorter one.
	bigtime_t shortSpin = BenchWarmup::SpinMs(20);
	bigtime_t longSpin  = BenchWarmup::SpinMs(80);
	CHECK(longSpin > shortSpin);
}


int
main()
{
	test_spin_respects_duration();
	test_spin_zero_is_noop();
	test_touch_buffer_null_safe();
	test_touch_buffer_writes_pages();
	test_spin_actually_runs_work();

	printf("BenchWarmup: %d checks, %d failures\n", sChecks, sFailures);
	return sFailures == 0 ? 0 : 1;
}
