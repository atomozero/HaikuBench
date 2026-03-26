/*
 * Copyright 2025 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 */


#include "CpuDatabase.h"

#include <OS.h>
#include <String.h>

#include <string.h>


struct CpuTDPEntry {
	const char*	substring;
	int32		tdp;
};


static const CpuTDPEntry kCpuTDPDatabase[] = {
	// Intel Core Ultra (Meteor Lake / Arrow Lake)
	{"Core Ultra 9 285",	65},
	{"Core Ultra 7 265",	65},
	{"Core Ultra 5 245",	65},
	{"Core Ultra 9 185H",	45},
	{"Core Ultra 7 165H",	28},
	{"Core Ultra 7 155H",	28},
	{"Core Ultra 5 135H",	28},
	{"Core Ultra 5 125H",	28},

	// Intel 14th Gen (Raptor Lake Refresh)
	{"i9-14900K",	125},
	{"i9-14900",	65},
	{"i7-14700K",	125},
	{"i7-14700",	65},
	{"i5-14600K",	125},
	{"i5-14600",	65},
	{"i5-14500",	65},
	{"i5-14400",	65},
	{"i3-14100",	60},

	// Intel 13th Gen (Raptor Lake)
	{"i9-13900K",	125},
	{"i9-13900",	65},
	{"i7-13700K",	125},
	{"i7-13700",	65},
	{"i5-13600K",	125},
	{"i5-13600",	65},
	{"i5-13500",	65},
	{"i5-13400",	65},
	{"i3-13100",	60},

	// Intel 12th Gen (Alder Lake)
	{"i9-12900K",	125},
	{"i9-12900",	65},
	{"i7-12700K",	125},
	{"i7-12700",	65},
	{"i5-12600K",	125},
	{"i5-12600",	65},
	{"i5-12500",	65},
	{"i5-12400",	65},
	{"i3-12100",	60},

	// Intel 11th Gen (Rocket Lake / Tiger Lake)
	{"i9-11900K",	125},
	{"i9-11900",	65},
	{"i7-11700K",	125},
	{"i7-11700",	65},
	{"i5-11600K",	125},
	{"i5-11600",	65},
	{"i5-11400",	65},
	{"i3-11100",	65},

	// Intel 10th Gen (Comet Lake)
	{"i9-10900K",	125},
	{"i9-10900",	65},
	{"i7-10700K",	125},
	{"i7-10700",	65},
	{"i5-10600K",	125},
	{"i5-10600",	65},
	{"i5-10500",	65},
	{"i5-10400",	65},
	{"i3-10100",	65},

	// Intel older gens
	{"i9-9900K",	95},
	{"i9-9900",		65},
	{"i7-9700K",	95},
	{"i7-9700",		65},
	{"i7-8700K",	95},
	{"i7-8700",		65},
	{"i7-7700K",	91},
	{"i7-7700",		65},
	{"i7-6700K",	91},
	{"i7-6700",		65},
	{"i7-4790K",	88},
	{"i7-4790",		84},
	{"i7-4770K",	84},
	{"i7-4770",		84},
	{"i7-3770K",	77},
	{"i7-3770",		77},
	{"i5-9600K",	95},
	{"i5-9400",		65},
	{"i5-8600K",	95},
	{"i5-8400",		65},
	{"i5-7600K",	91},
	{"i5-7500",		65},
	{"i5-6600K",	91},
	{"i5-6500",		65},
	{"i5-4690K",	88},
	{"i5-4590",		84},
	{"i5-4460",		84},
	{"i3-8100",		65},
	{"i3-7100",		51},

	// Intel mobile
	{"i7-1365U",	15},
	{"i7-1355U",	15},
	{"i7-1265U",	15},
	{"i7-1255U",	15},
	{"i7-1165G7",	28},
	{"i7-10710U",	15},
	{"i7-10510U",	15},
	{"i7-8565U",	15},
	{"i7-8550U",	15},
	{"i7-7500U",	15},
	{"i5-1345U",	15},
	{"i5-1335U",	15},
	{"i5-1245U",	15},
	{"i5-1235U",	15},
	{"i5-1135G7",	28},
	{"i5-10210U",	15},
	{"i5-8265U",	15},
	{"i5-8250U",	15},
	{"i5-7200U",	15},

	// Intel Xeon (common workstation/server)
	{"Xeon E-2388G",	80},
	{"Xeon E-2378G",	80},
	{"Xeon E-2356G",	80},
	{"Xeon E-2334",		65},
	{"Xeon E-2324G",	65},
	{"Xeon E-2286G",	95},
	{"Xeon E-2278G",	80},
	{"Xeon E-2276G",	80},
	{"Xeon E-2236",		80},
	{"Xeon E-2176G",	80},
	{"Xeon E-2174G",	71},
	{"Xeon E-2136",		80},
	{"Xeon E-2134",		70},
	{"Xeon E-2124",		71},
	{"Xeon E3-1275",	84},
	{"Xeon E3-1245",	84},
	{"Xeon E3-1230",	80},
	{"Xeon E3-1220",	80},

	// Intel Celeron / Pentium
	{"Celeron N5105",	10},
	{"Celeron N4500",	6},
	{"Celeron N4020",	6},
	{"Celeron J4125",	10},
	{"Celeron J4005",	10},
	{"Celeron J3455",	10},
	{"Celeron N3450",	6},
	{"Celeron N3350",	6},
	{"Pentium N6005",	10},
	{"Pentium N5030",	6},
	{"Pentium J5040",	10},

	// Intel Atom
	{"Atom x7-Z8750",	2},
	{"Atom x5-Z8350",	2},
	{"Atom x5-Z8300",	2},
	{"Atom Z3795",		2},

	// AMD Ryzen 9000 (Zen 5)
	{"Ryzen 9 9950X",	170},
	{"Ryzen 9 9900X",	120},
	{"Ryzen 7 9700X",	65},
	{"Ryzen 5 9600X",	65},

	// AMD Ryzen 7000 (Zen 4)
	{"Ryzen 9 7950X",	170},
	{"Ryzen 9 7900X",	170},
	{"Ryzen 9 7900",	65},
	{"Ryzen 7 7800X3D",	120},
	{"Ryzen 7 7700X",	105},
	{"Ryzen 7 7700",	65},
	{"Ryzen 5 7600X",	105},
	{"Ryzen 5 7600",	65},

	// AMD Ryzen 5000 (Zen 3)
	{"Ryzen 9 5950X",	105},
	{"Ryzen 9 5900X",	105},
	{"Ryzen 9 5900",	65},
	{"Ryzen 7 5800X3D",	105},
	{"Ryzen 7 5800X",	105},
	{"Ryzen 7 5800",	65},
	{"Ryzen 7 5700X",	65},
	{"Ryzen 7 5700G",	65},
	{"Ryzen 5 5600X",	65},
	{"Ryzen 5 5600",	65},
	{"Ryzen 5 5600G",	65},
	{"Ryzen 5 5500",	65},
	{"Ryzen 3 5300G",	65},

	// AMD Ryzen 3000 (Zen 2)
	{"Ryzen 9 3950X",	105},
	{"Ryzen 9 3900X",	105},
	{"Ryzen 7 3800X",	105},
	{"Ryzen 7 3700X",	65},
	{"Ryzen 5 3600X",	95},
	{"Ryzen 5 3600",	65},
	{"Ryzen 5 3500X",	65},
	{"Ryzen 3 3300X",	65},
	{"Ryzen 3 3100",	65},

	// AMD Ryzen 2000 (Zen+)
	{"Ryzen 7 2700X",	105},
	{"Ryzen 7 2700",	65},
	{"Ryzen 5 2600X",	95},
	{"Ryzen 5 2600",	65},
	{"Ryzen 5 2400G",	65},
	{"Ryzen 3 2200G",	65},

	// AMD Ryzen mobile
	{"Ryzen 9 7945HX",	55},
	{"Ryzen 9 6900HX",	45},
	{"Ryzen 7 7840U",	28},
	{"Ryzen 7 7840HS",	35},
	{"Ryzen 7 7735HS",	35},
	{"Ryzen 7 6800U",	15},
	{"Ryzen 7 6800H",	45},
	{"Ryzen 7 5825U",	15},
	{"Ryzen 7 5800U",	15},
	{"Ryzen 7 5800H",	45},
	{"Ryzen 7 5700U",	15},
	{"Ryzen 7 4800U",	15},
	{"Ryzen 7 4800H",	45},
	{"Ryzen 7 4700U",	15},
	{"Ryzen 5 7640U",	28},
	{"Ryzen 5 7535HS",	35},
	{"Ryzen 5 6600U",	15},
	{"Ryzen 5 5625U",	15},
	{"Ryzen 5 5600U",	15},
	{"Ryzen 5 5600H",	45},
	{"Ryzen 5 5500U",	15},
	{"Ryzen 5 4600U",	15},
	{"Ryzen 5 4600H",	45},
	{"Ryzen 5 4500U",	15},
	{"Ryzen 3 5425U",	15},
	{"Ryzen 3 5400U",	15},

	// AMD FX
	{"FX-9590",			220},
	{"FX-8370",			125},
	{"FX-8350",			125},
	{"FX-8320",			125},
	{"FX-6300",			95},
	{"FX-4300",			95},

	// AMD Athlon / A-series
	{"Athlon 3000G",	35},
	{"Athlon 200GE",	35},
	{"A10-7870K",		95},
	{"A10-7850K",		95},
	{"A8-7600",			65},
	{"A6-7400K",		65},

	// AMD EPYC (common)
	{"EPYC 9654",		360},
	{"EPYC 9554",		360},
	{"EPYC 7763",		280},
	{"EPYC 7713",		240},
	{"EPYC 7543",		225},
	{"EPYC 7453",		225},
	{"EPYC 7313",		155},
	{"EPYC 7282",		120},
	{"EPYC 7252",		120},

	// Generic fallbacks (match broad patterns last)
	{"Core i9",			125},
	{"Core i7",			65},
	{"Core i5",			65},
	{"Core i3",			65},
	{"Ryzen 9",			105},
	{"Ryzen 7",			65},
	{"Ryzen 5",			65},
	{"Ryzen 3",			65},
	{"Celeron",			15},
	{"Pentium",			15},
	{"Atom",			6},
	{"Xeon",			80},
	{"EPYC",			225},
	{"FX-",				125},
};

static const int32 kCpuTDPDatabaseSize
	= sizeof(kCpuTDPDatabase) / sizeof(kCpuTDPDatabase[0]);


/*static*/ int32
CpuDatabase::LookupTDP(const char* cpuModel)
{
	if (cpuModel == NULL)
		return -1;

	BString model(cpuModel);

	for (int32 i = 0; i < kCpuTDPDatabaseSize; i++) {
		if (model.FindFirst(kCpuTDPDatabase[i].substring) >= 0)
			return kCpuTDPDatabase[i].tdp;
	}

	return -1;
}


/*static*/ BString
CpuDatabase::GetCpuBrandString()
{
#if defined(__i386__) || defined(__x86_64__)
	cpuid_info info;
	char brand[49];
	memset(brand, 0, sizeof(brand));

	for (uint32 i = 0; i < 3; i++) {
		if (get_cpuid(&info, 0x80000002 + i, 0) != B_OK)
			return "Unknown CPU";

		// Haiku's cpuid_info.regs order is {eax, ebx, edx, ecx}
		// but brand string expects {eax, ebx, ecx, edx}
		uint32 offset = i * 16;
		memcpy(brand + offset, &info.regs.eax, 4);
		memcpy(brand + offset + 4, &info.regs.ebx, 4);
		memcpy(brand + offset + 8, &info.regs.ecx, 4);
		memcpy(brand + offset + 12, &info.regs.edx, 4);
	}

	brand[48] = '\0';

	// Trim leading spaces
	const char* trimmed = brand;
	while (*trimmed == ' ')
		trimmed++;

	// Collapse multiple spaces into one
	BString result;
	bool prevSpace = false;

	for (const char* p = trimmed; *p != '\0'; p++) {
		if (*p == ' ') {
			if (!prevSpace)
				result += ' ';
			prevSpace = true;
		} else {
			result += *p;
			prevSpace = false;
		}
	}

	// Trim trailing spaces
	result.Trim();

	return result;
#else
	return "Unknown CPU (non-x86)";
#endif
}
