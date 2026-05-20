/*
 * Copyright 2025 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef TEMP_OVERLAY_H
#define TEMP_OVERLAY_H


#include <atomic>

#include <SupportDefs.h>


class TempOverlay {
public:
	static	void				ReadTemperatures();
	static	void				DrawOverlay(float viewWidth,
									float viewHeight);

private:
	static	void				_DrawString(const char* str, float x,
									float y);
	static	void				_DrawBackground(float x, float y,
									float width, float height);

	static	const int32			kMaxZones = 8;
	static	float				sTemp[kMaxZones];
	static	float				sCritical[kMaxZones];
	static	int32				sZoneCount;
	static	bigtime_t			sLastReadTime;
	static	std::atomic_flag	sLock;
};


#endif // TEMP_OVERLAY_H
