/*
 * Copyright 2025 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 */


#include "SysBenchmark.h"

#include <atomic>

#include <Looper.h>
#include <Message.h>
#include <Messenger.h>
#include <OS.h>
#include <image.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>

#include "AdaptiveRunner.h"
#include "BenchStats.h"
#include "BenchWarmup.h"
#include "CpuDatabase.h"


static const float kTestDuration = 2.0f; // seconds per test
static const bigtime_t kTestUsecs = (bigtime_t)(kTestDuration * 1000000.0f);


static const char* kTestNames[] = {
	"CPU Integer",
	"CPU Float",
	"CPU Multi-thread",
	"RAM Seq Read",
	"RAM Seq Write",
	"RAM Copy",
	"RAM Latency",
	"Cache L1 (32K)",
	"Cache L2 (256K)",
	"Cache L3 (3M)",
	"Sem Create/Delete",
	"Sem Acquire/Release",
	"Sem Contention",
	"Thread Spawn",
	"Port Send/Recv",
	"Area Create/Delete",
	"Atomic Ops",
	"Syscall Overhead",
	"BMessage Flatten",
	"BLooper Messaging"
};


const char*
SysBenchmark::TestName(int32 index)
{
	if (index < 0 || index >= kNumTests)
		return "Unknown";
	return kTestNames[index];
}


float
SysBenchmark::ResultValue(const SysBenchResults& r, int32 index)
{
	const float* fields = &r.cpuIntegerMOPS;
	if (index < 0 || index >= kNumTests)
		return 0.0f;
	return fields[index];
}


SysBenchResults
SysBenchmark::Run(int32 currentTestCallback(int32, void*), void* cookie)
{
	SysBenchResults results;
	memset(&results, 0, sizeof(results));

	typedef float (*TestFunc)();
	TestFunc tests[] = {
		_TestCpuInteger,
		_TestCpuFloat,
		_TestCpuMulti,
		_TestRamSeqRead,
		_TestRamSeqWrite,
		_TestRamCopy,
		_TestRamLatency,
		_TestCacheL1,
		_TestCacheL2,
		_TestCacheL3,
		_TestSemCreateDelete,
		_TestSemAcquireRelease,
		_TestSemContention,
		_TestThreadSpawn,
		_TestPortSendRecv,
		_TestAreaCreateDelete,
		_TestAtomicOps,
		_TestSyscallOverhead,
		_TestBMessageFlatten,
		_TestBLooperMsg
	};

	float* resultFields = &results.cpuIntegerMOPS;

	// Adaptive sampling policy. Defaults mirror the old behaviour
	// (minRuns = 3) but allow the runner to take up to 10 samples
	// when CoV is still above 2%. This makes stable systems fast
	// and noisy ones more accurate, without changing the contract
	// for the simple case.
	AdaptiveConfig cfg;
	cfg.minRuns = kBenchRuns;
	cfg.maxRuns = 10;
	cfg.targetCoV = 0.02f;

	int32 maxRunsObserved = 0;

	for (int32 i = 0; i < kNumTests; i++) {
		if (currentTestCallback != NULL)
			currentTestCallback(i, cookie);

		// Warm-up before every test: pushes CPU frequency governor to
		// the top, resolves PLT/lazy-bound symbols, and pulls libc
		// math tables into cache. 200 ms is a conservative default
		// (see BenchWarmup::kDefaultWarmupMs).
		BenchWarmup::SpinMs(BenchWarmup::kDefaultWarmupMs);

		BenchStats s = AdaptiveRunner::Run(tests[i], cfg);
		results.stats[i] = s;

		// Populate legacy flat fields. Use the trimmed mean when
		// valid — it is identical to the raw mean on clean data and
		// strictly better when an outlier was detected.
		if (s.valid) {
			resultFields[i] = s.trimmedMean;
			results.stddev[i] = s.stddev;
			if (s.nSamples > maxRunsObserved)
				maxRunsObserved = s.nSamples;
		} else {
			resultFields[i] = -1.0f;
			results.stddev[i] = 0.0f;
		}
	}

	results.valid = true;
	results.runs = maxRunsObserved > 0 ? maxRunsObserved : kBenchRuns;
	return results;
}


BString
SysBenchmark::GetSystemDescription()
{
	system_info info;
	get_system_info(&info);

	BString cpu = CpuDatabase::GetCpuBrandString();
	uint64 ramMB = ((uint64)info.max_pages * B_PAGE_SIZE) / (1024 * 1024);

	BString desc;
	desc.SetToFormat("%s | %" B_PRIu32 " cores | %" B_PRIu64 " MB RAM",
		cpu.String(), info.cpu_count, ramMB);
	return desc;
}


BString
SysBenchmark::GetHaikuVersion()
{
	utsname uname_info;
	if (uname(&uname_info) == 0) {
		BString version;
		version.SetToFormat("%s %s", uname_info.sysname,
			uname_info.version);
		return version;
	}
	return BString("Haiku (unknown version)");
}


BString
SysBenchmark::GetMachineId()
{
	// Build a fingerprint string from hardware characteristics
	system_info info;
	get_system_info(&info);

	BString cpu = CpuDatabase::GetCpuBrandString();
	uint64 ramMB = ((uint64)info.max_pages * B_PAGE_SIZE) / (1024 * 1024);

	BString fingerprint;
	fingerprint.SetToFormat("%s|%" B_PRIu32 "|%" B_PRIu64,
		cpu.String(), info.cpu_count, ramMB);

	// FNV-1a 32-bit hash
	uint32 hash = 2166136261u;
	for (int32 i = 0; i < fingerprint.Length(); i++) {
		hash ^= (uint8)fingerprint[i];
		hash *= 16777619u;
	}

	// Format as 8-char hex ID
	BString id;
	id.SetToFormat("HB-%08" B_PRIX32, hash);
	return id;
}


// #pragma mark - CPU Tests


float
SysBenchmark::_TestCpuInteger()
{
	// Integer arithmetic: additions, multiplications, divisions, bit ops
	int64 a = 1, b = 2, c = 3;
	int64 ops = 0;

	bigtime_t start = system_time();
	bigtime_t deadline = start + kTestUsecs;

	while (system_time() < deadline) {
		for (int32 i = 0; i < 10000; i++) {
			a = a + b * c;
			b = (a ^ c) + (b >> 1);
			c = (a % (b | 1)) + (c << 1);
			a = a - c + (b & 0xFF);
			b = (c * a) ^ (b + 7);
			c = (a + b) / ((c & 0x7F) | 1);
			a = (b ^ c) * ((a & 0xFFF) + 1);
			b = a + c - (b >> 2);
		}
		__asm__ __volatile__("" : "+r"(a), "+r"(b), "+r"(c));
		ops += 80000; // 8 ops * 10000
	}

	bigtime_t elapsed = system_time() - start;
	return (float)ops / (float)elapsed; // millions ops/sec
}


struct CpuThreadData {
	std::atomic<bool>*	running;
	int64			ops;
};


static int32
_CpuWorker(void* data)
{
	CpuThreadData* td = static_cast<CpuThreadData*>(data);
	int64 a = 1, b = 2, c = 3;
	int64 ops = 0;

	while (td->running->load(std::memory_order_relaxed)) {
		for (int32 i = 0; i < 10000; i++) {
			a = a + b * c;
			b = (a ^ c) + (b >> 1);
			c = (a % (b | 1)) + (c << 1);
			a = a - c + (b & 0xFF);
			b = (c * a) ^ (b + 7);
			c = (a + b) / ((c & 0x7F) | 1);
			a = (b ^ c) * ((a & 0xFFF) + 1);
			b = a + c - (b >> 2);
		}
		__asm__ __volatile__("" : "+r"(a), "+r"(b), "+r"(c));
		ops += 80000;
	}

	td->ops = ops;
	return 0;
}


float
SysBenchmark::_TestCpuFloat()
{
	// Floating point: sin, cos, sqrt, multiply, add
	double a = 1.1, b = 2.2, c = 3.3;
	int64 ops = 0;

	bigtime_t start = system_time();
	bigtime_t deadline = start + kTestUsecs;

	while (system_time() < deadline) {
		for (int32 i = 0; i < 5000; i++) {
			a = sin(b) * cos(c) + a;
			b = sqrt(fabs(a * c)) + b * 0.99;
			c = a * b - c / (fabs(a) + 1.0);
			a = log(fabs(b) + 1.0) + c * 0.5;
			b = exp(fmod(a, 5.0)) * 0.001 + b;
			c = pow(fabs(a) + 1.0, 0.3) + sin(b);
		}
		__asm__ __volatile__("" : "+r"(a), "+r"(b), "+r"(c));
		ops += 30000; // 6 ops * 5000
	}

	bigtime_t elapsed = system_time() - start;
	return (float)ops / (float)elapsed; // MFLOPS
}


float
SysBenchmark::_TestCpuMulti()
{
	system_info info;
	get_system_info(&info);
	uint32 cpuCount = info.cpu_count;
	if (cpuCount < 1)
		cpuCount = 1;

	// First measure single-thread performance
	std::atomic<bool> singleRunning(true);
	CpuThreadData singleData;
	singleData.running = &singleRunning;
	singleData.ops = 0;

	thread_id singleThread = spawn_thread(_CpuWorker, "cpu_single",
		B_NORMAL_PRIORITY, &singleData);
	resume_thread(singleThread);
	snooze(kTestUsecs);
	singleRunning = false;
	status_t result;
	wait_for_thread(singleThread, &result);
	int64 singleOps = singleData.ops;

	// Now measure multi-thread (same duration)
	std::atomic<bool> multiRunning(true);
	CpuThreadData* multiData = new CpuThreadData[cpuCount];
	thread_id* threads = new thread_id[cpuCount];

	for (uint32 i = 0; i < cpuCount; i++) {
		multiData[i].running = &multiRunning;
		multiData[i].ops = 0;
		BString name;
		name.SetToFormat("cpu_multi_%" B_PRIu32, i);
		threads[i] = spawn_thread(_CpuWorker, name.String(),
			B_NORMAL_PRIORITY, &multiData[i]);
	}

	for (uint32 i = 0; i < cpuCount; i++)
		resume_thread(threads[i]);
	snooze(kTestUsecs);
	multiRunning = false;

	int64 totalOps = 0;
	for (uint32 i = 0; i < cpuCount; i++) {
		wait_for_thread(threads[i], &result);
		totalOps += multiData[i].ops;
	}

	delete[] multiData;
	delete[] threads;

	// Return speedup ratio (ideally == cpuCount)
	// Compare total multi-thread ops vs single-thread ops over same duration
	if (singleOps > 0)
		return (float)((double)totalOps / (double)singleOps);
	return 1.0f;
}


// #pragma mark - RAM Tests


float
SysBenchmark::_TestRamSeqRead()
{
	const size_t size = 32 * 1024 * 1024; // 32 MB
	volatile uint8* buffer = (volatile uint8*)malloc(size);
	if (buffer == NULL)
		return 0.0f;

	// Touch all pages
	memset((void*)buffer, 0xAA, size);

	volatile uint64 sink = 0;
	int64 totalBytes = 0;

	bigtime_t start = system_time();
	bigtime_t deadline = start + kTestUsecs;

	while (system_time() < deadline) {
		const volatile uint64* ptr = (const volatile uint64*)buffer;
		const volatile uint64* end = ptr + (size / sizeof(uint64));
		while (ptr < end) {
			sink += *ptr;
			ptr += 8; // stride to avoid prefetcher saturation
		}
		totalBytes += size;
	}

	bigtime_t elapsed = system_time() - start;
	free((void*)buffer);

	(void)sink;
	if (elapsed < 1000)
		elapsed = 1000;
	return (float)((double)totalBytes / (double)elapsed);
}


float
SysBenchmark::_TestRamSeqWrite()
{
	const size_t size = 32 * 1024 * 1024;
	volatile uint8* buffer = (volatile uint8*)malloc(size);
	if (buffer == NULL)
		return 0.0f;

	memset((void*)buffer, 0, size);

	int64 totalBytes = 0;

	bigtime_t start = system_time();
	bigtime_t deadline = start + kTestUsecs;

	while (system_time() < deadline) {
		volatile uint64* ptr = (volatile uint64*)buffer;
		volatile uint64* end = ptr + (size / sizeof(uint64));
		uint64 val = 0x5555555555555555ULL;
		while (ptr < end) {
			*ptr = val;
			ptr += 8;
		}
		totalBytes += size;
	}

	bigtime_t elapsed = system_time() - start;
	free((void*)buffer);

	if (elapsed < 1000)
		elapsed = 1000;
	return (float)((double)totalBytes / (double)elapsed);
}


float
SysBenchmark::_TestRamCopy()
{
	const size_t size = 32 * 1024 * 1024;
	uint8* src = (uint8*)malloc(size);
	uint8* dst = (uint8*)malloc(size);
	if (src == NULL || dst == NULL) {
		free(src);
		free(dst);
		return -1.0f;
	}

	memset(src, 0xAA, size);
	memset(dst, 0, size);

	int32 iterations = 0;

	bigtime_t start = system_time();
	bigtime_t deadline = start + kTestUsecs;

	while (system_time() < deadline) {
		memcpy(dst, src, size);
		// Prevent the compiler from optimizing away the memcpy
		__asm__ __volatile__("" : : "r"(dst[0]) : "memory");
		iterations++;
	}

	bigtime_t elapsed = system_time() - start;
	free(src);
	free(dst);

	if (elapsed < 1000)
		return -1.0f;

	// iterations * size = total bytes, / elapsed(usec) ≈ MB/s
	return (float)((double)iterations * (double)size / (double)elapsed);
}


float
SysBenchmark::_TestRamLatency()
{
	// Pointer-chasing latency test
	// Use 512K entries (4MB on 64-bit) — enough to exceed L3 cache
	const size_t count = 512 * 1024;
	void** chain = (void**)malloc(count * sizeof(void*));
	if (chain == NULL)
		return -1.0f;

	// Build a random pointer chain to defeat prefetchers
	// Fisher-Yates shuffle of indices
	uint32* indices = (uint32*)malloc(count * sizeof(uint32));
	if (indices == NULL) {
		free(chain);
		return -1.0f;
	}

	for (size_t i = 0; i < count; i++)
		indices[i] = (uint32)i;

	uint32 seed = 12345;
	for (size_t i = count - 1; i > 0; i--) {
		seed = seed * 1103515245 + 12345;
		size_t j = seed % (i + 1);
		uint32 tmp = indices[i];
		indices[i] = indices[j];
		indices[j] = tmp;
	}

	for (size_t i = 0; i < count - 1; i++)
		chain[indices[i]] = &chain[indices[i + 1]];
	chain[indices[count - 1]] = &chain[indices[0]];

	size_t chainStart = indices[0];
	free(indices);

	// Warm up the chain
	void** p = &chain[chainStart];
	for (size_t i = 0; i < count; i++) {
		p = (void**)*p;
		__asm__ __volatile__("" : "+r"(p));
	}

	// Chase the pointers — asm barrier prevents the compiler from
	// optimizing away the dependent loads
	p = &chain[chainStart];
	int64 chases = 0;

	bigtime_t start = system_time();
	bigtime_t deadline = start + kTestUsecs;

	while (system_time() < deadline) {
		for (int32 i = 0; i < 10000; i++) {
			p = (void**)*p;
			__asm__ __volatile__("" : "+r"(p));
		}
		chases += 10000;
	}

	bigtime_t elapsed = system_time() - start;
	free(chain);

	if (chases <= 0 || elapsed <= 0)
		return -1.0f;

	// Return latency in nanoseconds per chase
	return (float)((double)elapsed * 1000.0 / (double)chases);
}


// #pragma mark - Semaphore & Threading Tests


float
SysBenchmark::_TestSemCreateDelete()
{
	int64 ops = 0;

	bigtime_t start = system_time();
	bigtime_t deadline = start + kTestUsecs;

	while (system_time() < deadline) {
		for (int32 i = 0; i < 1000; i++) {
			sem_id sem = create_sem(0, "bench_sem");
			if (sem >= 0)
				delete_sem(sem);
		}
		ops += 1000;
	}

	bigtime_t elapsed = system_time() - start;
	return (float)ops * 1000.0f / (float)elapsed; // kOPS (ops/usec * 1000)
}


float
SysBenchmark::_TestSemAcquireRelease()
{
	sem_id sem = create_sem(1, "bench_acqrel");
	if (sem < 0)
		return 0.0f;

	int64 ops = 0;

	bigtime_t start = system_time();
	bigtime_t deadline = start + kTestUsecs;

	while (system_time() < deadline) {
		for (int32 i = 0; i < 10000; i++) {
			acquire_sem(sem);
			release_sem(sem);
		}
		ops += 10000;
	}

	bigtime_t elapsed = system_time() - start;
	delete_sem(sem);

	return (float)ops * 1000.0f / (float)elapsed;
}


struct SemContentionData {
	sem_id			sem;
	std::atomic<bool>*	running;
	int64			ops;
};


static int32
_SemContentionWorker(void* data)
{
	SemContentionData* sd = static_cast<SemContentionData*>(data);
	int64 ops = 0;

	while (sd->running->load(std::memory_order_relaxed)) {
		for (int32 i = 0; i < 1000; i++) {
			acquire_sem(sd->sem);
			release_sem(sd->sem);
		}
		ops += 1000;
	}

	sd->ops = ops;
	return 0;
}


float
SysBenchmark::_TestSemContention()
{
	system_info info;
	get_system_info(&info);
	uint32 cpuCount = info.cpu_count;
	if (cpuCount < 2)
		cpuCount = 2;

	sem_id sem = create_sem(1, "bench_contention");
	if (sem < 0)
		return 0.0f;

	std::atomic<bool> running(true);
	SemContentionData* data = new SemContentionData[cpuCount];
	thread_id* threads = new thread_id[cpuCount];

	for (uint32 i = 0; i < cpuCount; i++) {
		data[i].sem = sem;
		data[i].running = &running;
		data[i].ops = 0;
		BString name;
		name.SetToFormat("sem_contend_%" B_PRIu32, i);
		threads[i] = spawn_thread(_SemContentionWorker, name.String(),
			B_NORMAL_PRIORITY, &data[i]);
	}

	for (uint32 i = 0; i < cpuCount; i++)
		resume_thread(threads[i]);

	snooze(kTestUsecs);
	running = false;

	int64 totalOps = 0;
	for (uint32 i = 0; i < cpuCount; i++) {
		status_t result;
		wait_for_thread(threads[i], &result);
		totalOps += data[i].ops;
	}

	bigtime_t elapsed = kTestUsecs;
	delete_sem(sem);
	delete[] data;
	delete[] threads;

	return (float)totalOps * 1000.0f / (float)elapsed;
}


float
SysBenchmark::_TestThreadSpawn()
{
	int64 ops = 0;

	bigtime_t start = system_time();
	bigtime_t deadline = start + kTestUsecs;

	while (system_time() < deadline) {
		for (int32 i = 0; i < 100; i++) {
			thread_id t = spawn_thread(
				[](void*) -> int32 { return 0; },
				"bench_thread", B_NORMAL_PRIORITY, NULL);
			if (t >= 0) {
				resume_thread(t);
				status_t result;
				wait_for_thread(t, &result);
			}
		}
		ops += 100;
	}

	bigtime_t elapsed = system_time() - start;
	return (float)ops * 1000.0f / (float)elapsed;
}


float
SysBenchmark::_TestPortSendRecv()
{
	port_id port = create_port(256, "bench_port");
	if (port < 0)
		return 0.0f;

	int32 data = 42;
	int64 ops = 0;

	bigtime_t start = system_time();
	bigtime_t deadline = start + kTestUsecs;

	while (system_time() < deadline) {
		for (int32 i = 0; i < 1000; i++) {
			write_port(port, 'test', &data, sizeof(data));
			int32 code;
			int32 buf;
			read_port(port, &code, &buf, sizeof(buf));
		}
		ops += 1000;
	}

	bigtime_t elapsed = system_time() - start;
	delete_port(port);

	return (float)ops * 1000.0f / (float)elapsed;
}


float
SysBenchmark::_TestAreaCreateDelete()
{
	int64 ops = 0;

	bigtime_t start = system_time();
	bigtime_t deadline = start + kTestUsecs;

	while (system_time() < deadline) {
		for (int32 i = 0; i < 500; i++) {
			void* addr;
			area_id area = create_area("bench_area", &addr,
				B_ANY_ADDRESS, B_PAGE_SIZE,
				B_NO_LOCK, B_READ_AREA | B_WRITE_AREA);
			if (area >= 0)
				delete_area(area);
		}
		ops += 500;
	}

	bigtime_t elapsed = system_time() - start;
	return (float)ops * 1000.0f / (float)elapsed;
}


float
SysBenchmark::_TestAtomicOps()
{
	int32 counter = 0;
	int64 ops = 0;

	bigtime_t start = system_time();
	bigtime_t deadline = start + kTestUsecs;

	while (system_time() < deadline) {
		for (int32 i = 0; i < 100000; i++) {
			atomic_add(&counter, 1);
			atomic_add(&counter, -1);
			atomic_or(&counter, 0xFF);
			atomic_and(&counter, 0);
		}
		ops += 400000; // 4 ops * 100000
	}

	bigtime_t elapsed = system_time() - start;
	return (float)ops * 1000.0f / (float)elapsed; // kOPS
}


float
SysBenchmark::_TestSyscallOverhead()
{
	// Measure the cost of the lightest syscall: system_time()
	int64 calls = 0;

	bigtime_t start = system_time();
	bigtime_t deadline = start + kTestUsecs;

	while (system_time() < deadline) {
		for (int32 i = 0; i < 100000; i++) {
			(void)system_time();
		}
		calls += 100000;
	}

	bigtime_t elapsed = system_time() - start;

	// Return latency in nanoseconds per call
	if (calls > 0)
		return (float)elapsed * 1000.0f / (float)calls;
	return 0.0f;
}


// #pragma mark - Cache Tests


static float
_TestCacheBandwidth(size_t bufferSize)
{
	uint8* buffer = (uint8*)malloc(bufferSize);
	if (buffer == NULL)
		return -1.0f;

	// Fill buffer to ensure pages are mapped
	memset(buffer, 0xAA, bufferSize);

	const size_t count = bufferSize / sizeof(uint64);
	const uint64* base = (const uint64*)buffer;
	int64 totalBytes = 0;

	// Warm up: iterate a few times to pull data into the target cache level
	for (int32 w = 0; w < 4; w++) {
		uint64 s = 0;
		for (size_t i = 0; i < count; i += 8) {
			s += base[i] + base[i+1] + base[i+2] + base[i+3]
			   + base[i+4] + base[i+5] + base[i+6] + base[i+7];
		}
		__asm__ __volatile__("" : "+r"(s));
	}

	// Determine how many passes to batch between time checks
	// so that system_time() overhead is negligible
	int32 passesPerCheck = 1;
	if (bufferSize <= 32 * 1024)
		passesPerCheck = 64;
	else if (bufferSize <= 256 * 1024)
		passesPerCheck = 8;

	static const bigtime_t kCacheTestUsecs = 1500000; // 1.5 sec
	bigtime_t start = system_time();
	bigtime_t deadline = start + kCacheTestUsecs;

	while (system_time() < deadline) {
		for (int32 pass = 0; pass < passesPerCheck; pass++) {
			// 8 independent accumulators to allow multiple loads in flight
			uint64 s0 = 0, s1 = 0, s2 = 0, s3 = 0;
			uint64 s4 = 0, s5 = 0, s6 = 0, s7 = 0;
			for (size_t i = 0; i < count; i += 8) {
				s0 += base[i];
				s1 += base[i+1];
				s2 += base[i+2];
				s3 += base[i+3];
				s4 += base[i+4];
				s5 += base[i+5];
				s6 += base[i+6];
				s7 += base[i+7];
			}
			// Compiler barrier: accumulators are inputs/outputs,
			// preventing the compiler from optimizing away the reads
			__asm__ __volatile__("" : "+r"(s0), "+r"(s1), "+r"(s2), "+r"(s3));
			__asm__ __volatile__("" : "+r"(s4), "+r"(s5), "+r"(s6), "+r"(s7));
		}
		totalBytes += (int64)passesPerCheck * bufferSize;
	}

	bigtime_t elapsed = system_time() - start;
	free(buffer);

	if (elapsed < 1000)
		return -1.0f;
	return (float)((double)totalBytes / (double)elapsed);
}


float
SysBenchmark::_TestCacheL1()
{
	// L1 data cache: typically 32KB per core
	return _TestCacheBandwidth(24 * 1024);
}


float
SysBenchmark::_TestCacheL2()
{
	// L2 cache: typically 256KB per core
	return _TestCacheBandwidth(192 * 1024);
}


float
SysBenchmark::_TestCacheL3()
{
	// L3 cache: typically 3-8MB shared
	return _TestCacheBandwidth(2 * 1024 * 1024);
}


// #pragma mark - Messaging Tests


float
SysBenchmark::_TestBMessageFlatten()
{
	// Build a moderately complex BMessage
	BMessage msg('test');
	msg.AddString("name", "benchmark test message");
	msg.AddInt32("value", 42);
	msg.AddFloat("score", 3.14159f);
	msg.AddBool("active", true);
	msg.AddRect("frame", BRect(10, 20, 300, 200));
	for (int32 i = 0; i < 10; i++)
		msg.AddInt32("array", i * 100);

	// Pre-allocate flatten buffer
	ssize_t flatSize = msg.FlattenedSize();
	char* flatBuffer = new char[flatSize];

	int64 ops = 0;

	bigtime_t start = system_time();
	bigtime_t deadline = start + kTestUsecs;

	while (system_time() < deadline) {
		for (int32 i = 0; i < 1000; i++) {
			msg.Flatten(flatBuffer, flatSize);
			BMessage restored;
			restored.Unflatten(flatBuffer);
		}
		ops += 1000;
	}

	bigtime_t elapsed = system_time() - start;
	delete[] flatBuffer;

	return (float)ops * 1000.0f / (float)elapsed; // kOPS
}


// Helper looper for BLooper messaging test
class BenchLooper : public BLooper {
public:
	BenchLooper(int64* counter, std::atomic<bool>* running)
		: BLooper("BenchLooper"),
		  fCounter(counter),
		  fRunning(running)
	{
	}

	void MessageReceived(BMessage* message) {
		if (message->what == 'ping') {
			atomic_add64(fCounter, 1);
			if (fRunning->load(std::memory_order_relaxed)) {
				// Echo back
				BMessenger sender;
				if (message->FindMessenger("reply_to", &sender) == B_OK)
					sender.SendMessage(message);
			}
		} else {
			BLooper::MessageReceived(message);
		}
	}

private:
	int64*				fCounter;
	std::atomic<bool>*	fRunning;
};


float
SysBenchmark::_TestBLooperMsg()
{
	int64 counter = 0;
	std::atomic<bool> running(true);

	// Create two loopers that ping-pong messages
	BenchLooper* looperA = new BenchLooper(&counter, &running);
	BenchLooper* looperB = new BenchLooper(&counter, &running);
	looperA->Run();
	looperB->Run();

	BMessenger messengerA(NULL, looperA);
	BMessenger messengerB(NULL, looperB);

	// Seed the ping-pong: A sends to B with reply_to = A
	BMessage ping('ping');
	ping.AddMessenger("reply_to", messengerA);

	// Also set up B -> A direction
	BMessage ping2('ping');
	ping2.AddMessenger("reply_to", messengerB);

	// Send initial messages to start the ping-pong
	messengerB.SendMessage(&ping);
	messengerA.SendMessage(&ping2);

	bigtime_t start = system_time();
	snooze(kTestUsecs);
	running = false;

	// Allow loopers to drain
	snooze(50000);

	int64 totalOps = counter;
	bigtime_t elapsed = system_time() - start;

	// Clean up loopers
	looperA->Lock();
	looperA->Quit();
	looperB->Lock();
	looperB->Quit();

	return (float)totalOps * 1000.0f / (float)elapsed; // kOPS
}
