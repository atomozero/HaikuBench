/*
 * Copyright 2025 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 */


#include "StatsCollector.h"

#include <Message.h>
#include <OS.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


StatsCollector::StatsCollector(BMessenger target)
	:
	BLooper("StatsCollector"),
	fTarget(target),
	fThread(-1),
	fRunning(false),
	fTDP(65),
	fCpuUsage(0.0f),
	fUsedMemoryMB(0),
	fTotalMemoryMB(0),
	fPowerWatts(0.0f),
	fBootTime(0),
	fPrevActiveTime(NULL),
	fCpuCount(0),
	fThermalZoneCount(0)
{
	memset(fThermalTemp, 0, sizeof(fThermalTemp));
	memset(fThermalCritical, 0, sizeof(fThermalCritical));

	system_info sysInfo;
	if (get_system_info(&sysInfo) == B_OK) {
		fCpuCount = sysInfo.cpu_count;
		fBootTime = sysInfo.boot_time;
	}

	if (fCpuCount > 0) {
		fPrevActiveTime = new bigtime_t[fCpuCount];
		memset(fPrevActiveTime, 0, sizeof(bigtime_t) * fCpuCount);

		// Initialize previous active times
		for (uint32 i = 0; i < fCpuCount; i++) {
			cpu_info cpuInfo;
			if (get_cpu_info(i, 1, &cpuInfo) == B_OK)
				fPrevActiveTime[i] = cpuInfo.active_time;
		}
	}
}


StatsCollector::~StatsCollector()
{
	_StopCollecting();
	delete[] fPrevActiveTime;
}


void
StatsCollector::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgStatsStart:
			_StartCollecting();
			break;

		case kMsgStatsStop:
			_StopCollecting();
			break;

		default:
			BLooper::MessageReceived(message);
			break;
	}
}


void
StatsCollector::_CollectStats()
{
	system_info sysInfo;
	if (get_system_info(&sysInfo) != B_OK)
		return;

	// Memory
	uint64 pageSize = B_PAGE_SIZE;
	fTotalMemoryMB = (sysInfo.max_pages * pageSize) / (1024 * 1024);
	fUsedMemoryMB = (sysInfo.used_pages * pageSize) / (1024 * 1024);

	// CPU usage
	bigtime_t now = system_time();
	static bigtime_t sPrevTime = 0;
	bigtime_t elapsed = now - sPrevTime;

	if (sPrevTime > 0 && elapsed > 0) {
		float totalUsage = 0.0f;

		for (uint32 i = 0; i < fCpuCount; i++) {
			cpu_info cpuInfo;
			if (get_cpu_info(i, 1, &cpuInfo) == B_OK) {
				bigtime_t deltaActive
					= cpuInfo.active_time - fPrevActiveTime[i];
				float usage
					= (float)deltaActive / (float)elapsed * 100.0f;
				if (usage > 100.0f)
					usage = 100.0f;
				totalUsage += usage;
				fPrevActiveTime[i] = cpuInfo.active_time;
			}
		}

		fCpuUsage = totalUsage / fCpuCount;
	} else {
		// First collection, initialize previous times
		for (uint32 i = 0; i < fCpuCount; i++) {
			cpu_info cpuInfo;
			if (get_cpu_info(i, 1, &cpuInfo) == B_OK)
				fPrevActiveTime[i] = cpuInfo.active_time;
		}
	}

	sPrevTime = now;

	// Estimated power consumption: CPU_load% * TDP / 100
	fPowerWatts = fCpuUsage * (float)fTDP / 100.0f;

	// Read thermal zones
	_ReadThermalZones();

	// Send update to target
	BMessage update(kMsgStatsUpdate);
	update.AddFloat("cpu_usage", fCpuUsage);
	update.AddUInt64("used_memory_mb", fUsedMemoryMB);
	update.AddUInt64("total_memory_mb", fTotalMemoryMB);
	update.AddFloat("power_watts", fPowerWatts);
	update.AddInt64("boot_time", fBootTime);
	update.AddInt32("thermal_zone_count", fThermalZoneCount);
	for (int32 i = 0; i < fThermalZoneCount; i++) {
		update.AddFloat("thermal_temp", fThermalTemp[i]);
		update.AddFloat("thermal_critical", fThermalCritical[i]);
	}
	fTarget.SendMessage(&update);
}


float
StatsCollector::ThermalTemp(int32 zone) const
{
	if (zone < 0 || zone >= fThermalZoneCount)
		return -1.0f;
	return fThermalTemp[zone];
}


float
StatsCollector::ThermalCritical(int32 zone) const
{
	if (zone < 0 || zone >= fThermalZoneCount)
		return -1.0f;
	return fThermalCritical[zone];
}


void
StatsCollector::_ReadThermalZones()
{
	fThermalZoneCount = 0;

	for (int32 i = 0; i < kMaxThermalZones; i++) {
		char path[128];
		snprintf(path, sizeof(path), "/dev/power/acpi_thermal/%"
			B_PRId32, i);

		FILE* file = fopen(path, "r");
		if (file == NULL)
			break;

		char buffer[256];
		size_t bytesRead = fread(buffer, 1, sizeof(buffer) - 1, file);
		fclose(file);

		if (bytesRead == 0)
			break;

		buffer[bytesRead] = '\0';

		// Parse "Current Temperature: XX.X" and
		// "Critical Temperature: XX.X"
		float current = -1.0f;
		float critical = -1.0f;

		const char* critPtr = strstr(buffer, "Critical Temperature:");
		if (critPtr != NULL)
			sscanf(critPtr, "Critical Temperature: %f", &critical);

		const char* curPtr = strstr(buffer, "Current Temperature:");
		if (curPtr != NULL)
			sscanf(curPtr, "Current Temperature: %f", &current);

		if (current >= 0.0f) {
			fThermalTemp[i] = current;
			fThermalCritical[i] = critical;
			fThermalZoneCount = i + 1;
		}
	}
}


/*static*/ int32
StatsCollector::_CollectorThread(void* data)
{
	StatsCollector* collector = static_cast<StatsCollector*>(data);

	while (collector->fRunning) {
		collector->_CollectStats();
		snooze(2000000); // 2 seconds
	}

	return 0;
}


void
StatsCollector::_StartCollecting()
{
	if (fRunning)
		return;

	fRunning = true;
	fThread = spawn_thread(_CollectorThread, "stats_collector",
		B_LOW_PRIORITY, this);

	if (fThread >= 0)
		resume_thread(fThread);
}


void
StatsCollector::_StopCollecting()
{
	if (!fRunning)
		return;

	fRunning = false;

	if (fThread >= 0) {
		status_t result;
		wait_for_thread(fThread, &result);
		fThread = -1;
	}
}
