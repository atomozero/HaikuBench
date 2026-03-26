/*
 * Copyright 2025 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef CPU_DATABASE_H
#define CPU_DATABASE_H


#include <String.h>


class CpuDatabase {
public:
	static	int32				LookupTDP(const char* cpuModel);
	static	BString				GetCpuBrandString();
};


#endif // CPU_DATABASE_H
