/*
 * Copyright 2025 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 */


#include "BenchWarmup.h"


bigtime_t
BenchWarmup::SpinMs(int32 durationMs)
{
	if (durationMs <= 0)
		return 0;

	bigtime_t start = system_time();
	bigtime_t deadline = start + (bigtime_t)durationMs * 1000;
	volatile uint64 acc = 0;

	// Unrolled integer busy-loop. The asm barrier keeps `acc` alive
	// across iterations so -O2 cannot delete the body entirely.
	while (system_time() < deadline) {
		for (int32 i = 0; i < 10000; i++) {
			acc += (uint64)i * 2654435761ULL;
			acc ^= (acc >> 13);
			acc += 0x9E3779B97F4A7C15ULL;
		}
		__asm__ __volatile__("" : "+r"(acc));
	}

	return system_time() - start;
}


void
BenchWarmup::TouchBuffer(void* buffer, size_t size)
{
	if (buffer == NULL || size == 0)
		return;

	// B_PAGE_SIZE on x86/x86_64 Haiku is 4096. We stride by 4096 to
	// touch one byte per page, which is enough to map it. Writing,
	// not reading, so copy-on-write zero pages are actually allocated.
	static const size_t kStride = 4096;
	uint8* p = (uint8*)buffer;
	for (size_t off = 0; off < size; off += kStride)
		p[off] = (uint8)(off & 0xFF);
	// Also touch the last byte, in case size is not a multiple of stride.
	p[size - 1] = (uint8)((size - 1) & 0xFF);
}
