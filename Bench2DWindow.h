/*
 * Copyright 2025 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef BENCH_2D_WINDOW_H
#define BENCH_2D_WINDOW_H


#include <MessageRunner.h>
#include <Messenger.h>
#include <StringView.h>
#include <View.h>
#include <Window.h>


enum {
	kMsgBench2DResult	= 'b2rs',
	kMsgBench2DStart	= 'b2st',
	kMsgBench2DTempUpd	= 'b2tm'
};


struct Accel2DInfo {
	BString		accelerantName;		// e.g., "intel_extreme"
	BString		deviceName;			// from accelerant_device_info::name
	BString		chipset;			// from accelerant_device_info::chipset
	uint32		vramBytes;			// VRAM in bytes
	bool		isHardwareAccel;	// true if native driver with 2D hooks
	bool		hasFillRect;		// B_FILL_RECTANGLE hook present
	bool		hasScreenBlit;		// B_SCREEN_TO_SCREEN_BLIT hook present
	bool		hasInvertRect;		// B_INVERT_RECTANGLE hook present
	bool		hooksChecked;		// true if clone detection succeeded
};


static const int32 kNumBench2DTests = 24;
static const int32 kNumBench2DLevels = 6;


struct Bench2DResults {
	float	opsPerSec[kNumBench2DTests];
	float	mbPerSec[kNumBench2DTests];
	bool	valid;
};


class Bench2DView : public BView {
public:
								Bench2DView();

	virtual	void				AttachedToWindow();
	virtual	void				Draw(BRect updateRect);

			void				RunBenchmark();
			Bench2DResults		Results() const { return fResults; }
			BString				DriverInfo() const { return fDriverInfo; }
			Accel2DInfo			AccelInfo() const { return fAccelInfo; }

private:
			void				_DetectDriver();
			void				_DetectAcceleration();
			void				_RunTest(int32 testIndex);

			Bench2DResults		fResults;
			BString				fDriverInfo;
			Accel2DInfo			fAccelInfo;
};


class Bench2DWindow : public BWindow {
public:
								Bench2DWindow(BMessenger target);
	virtual						~Bench2DWindow();

	virtual	void				MessageReceived(BMessage* message);
	virtual	bool				QuitRequested();

private:
			void				_UpdateResultLabels();
			void				_UpdateTemperature();

			Bench2DView*		fBenchView;
			Bench2DResults		fCachedResults;
			Accel2DInfo			fCachedAccelInfo;
			BString				fCachedDriverInfo;
			BMessenger			fTarget;

			BStringView*		fDriverLabel;
			BStringView*		fDeviceLabel;
			BStringView*		fAccelStatusLabel;
			BStringView*		fHooksLabel;
			BStringView*		fLevelLabels[kNumBench2DLevels];
			BStringView*		fTestLabels[kNumBench2DTests];
			BStringView*		fStatusLabel;
			BStringView*		fTempLabel;
			BMessageRunner*		fTempRunner;
};


#endif // BENCH_2D_WINDOW_H
