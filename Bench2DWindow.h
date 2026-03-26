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

private:
			void				_DetectDriver();
			void				_RunTest(int32 testIndex);

			Bench2DResults		fResults;
			BString				fDriverInfo;
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
			BMessenger			fTarget;

			BStringView*		fDriverLabel;
			BStringView*		fLevelLabels[kNumBench2DLevels];
			BStringView*		fTestLabels[kNumBench2DTests];
			BStringView*		fStatusLabel;
			BStringView*		fTempLabel;
			BMessageRunner*		fTempRunner;
};


#endif // BENCH_2D_WINDOW_H
