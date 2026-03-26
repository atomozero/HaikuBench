/*
 * Copyright 2025 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef STATS_COLLECTOR_H
#define STATS_COLLECTOR_H


#include <Looper.h>
#include <Messenger.h>
#include <OS.h>
#include <String.h>


enum {
	kMsgStatsUpdate		= 'stup',
	kMsgStatsStart		= 'stst',
	kMsgStatsStop		= 'stsp'
};


class StatsCollector : public BLooper {
public:
								StatsCollector(BMessenger target);
	virtual						~StatsCollector();

	virtual	void				MessageReceived(BMessage* message);

			float				CpuUsage() const { return fCpuUsage; }
			uint64				UsedMemoryMB() const { return fUsedMemoryMB; }
			uint64				TotalMemoryMB() const { return fTotalMemoryMB; }
			float				PowerWatts() const { return fPowerWatts; }
			bigtime_t			BootTime() const { return fBootTime; }
			float				ThermalTemp(int32 zone) const;
			float				ThermalCritical(int32 zone) const;
			int32				ThermalZoneCount() const
									{ return fThermalZoneCount; }

			void				SetTDP(int32 tdp) { fTDP = tdp; }
			int32				TDP() const { return fTDP; }

private:
			void				_CollectStats();
			void				_ReadThermalZones();
	static	int32				_CollectorThread(void* data);
			void				_StartCollecting();
			void				_StopCollecting();

			BMessenger			fTarget;
			thread_id			fThread;
			volatile bool		fRunning;
			int32				fTDP;

			float				fCpuUsage;
			uint64				fUsedMemoryMB;
			uint64				fTotalMemoryMB;
			float				fPowerWatts;
			bigtime_t			fBootTime;

			bigtime_t*			fPrevActiveTime;
			uint32				fCpuCount;

	static	const int32			kMaxThermalZones = 8;
			float				fThermalTemp[kMaxThermalZones];
			float				fThermalCritical[kMaxThermalZones];
			int32				fThermalZoneCount;
};


#endif // STATS_COLLECTOR_H
