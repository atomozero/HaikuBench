/*
 * Copyright 2025 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef SYS_BENCHMARK_H
#define SYS_BENCHMARK_H


#include <OS.h>
#include <String.h>


static const int32 kBenchRuns = 3;


struct SysBenchResults {
	// CPU (3)
	float	cpuIntegerMOPS;
	float	cpuFloatMFLOPS;
	float	cpuMultiScore;

	// RAM (4)
	float	ramSeqReadMBs;
	float	ramSeqWriteMBs;
	float	ramCopyMBs;
	float	ramLatencyNs;

	// Cache (3)
	float	cacheL1MBs;
	float	cacheL2MBs;
	float	cacheL3MBs;

	// Kernel (8)
	float	semCreateDeleteKOPS;
	float	semAcquireReleaseKOPS;
	float	semContentionKOPS;
	float	threadSpawnKOPS;
	float	portSendRecvKOPS;
	float	areaCreateDeleteKOPS;
	float	atomicOpsKOPS;
	float	syscallOverheadNs;

	// Messaging (2)
	float	bmsgFlattenKOPS;
	float	blooperMsgKOPS;

	// Statistics (stddev for each test, same order)
	float	stddev[20];

	bool	valid;
	int32	runs;
};


class SysBenchmark {
public:
	static	SysBenchResults		Run(int32 currentTestCallback(int32 test,
									void* cookie) = NULL,
									void* cookie = NULL);

	static	BString				GetSystemDescription();
	static	BString				GetHaikuVersion();
	static	BString				GetMachineId();

	static	const int32			kNumTests = 20;
	static	const char*			TestName(int32 index);

	// Access mean values as array (same order as struct fields)
	static	float				ResultValue(const SysBenchResults& r,
									int32 index);

private:
	// CPU
	static	float				_TestCpuInteger();
	static	float				_TestCpuFloat();
	static	float				_TestCpuMulti();

	// RAM
	static	float				_TestRamSeqRead();
	static	float				_TestRamSeqWrite();
	static	float				_TestRamCopy();
	static	float				_TestRamLatency();

	// Cache
	static	float				_TestCacheL1();
	static	float				_TestCacheL2();
	static	float				_TestCacheL3();

	// Kernel
	static	float				_TestSemCreateDelete();
	static	float				_TestSemAcquireRelease();
	static	float				_TestSemContention();
	static	float				_TestThreadSpawn();
	static	float				_TestPortSendRecv();
	static	float				_TestAreaCreateDelete();
	static	float				_TestAtomicOps();
	static	float				_TestSyscallOverhead();

	// Messaging
	static	float				_TestBMessageFlatten();
	static	float				_TestBLooperMsg();
};


#endif // SYS_BENCHMARK_H
