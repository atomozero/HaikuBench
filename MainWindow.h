/*
 * Copyright 2025 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H


#include <Button.h>
#include <MessageRunner.h>
#include <StringView.h>
#include <Window.h>

#include "SysBenchmark.h"


enum {
	kMsgRunSysBench		= 'rnsb',
	kMsgRunBenchmark	= 'rnbm',
	kMsgRun2DBenchmark	= 'rn2d',
	kMsgRunGpuBenchmark	= 'rngp',
	kMsgRunVkBenchmark	= 'rnvk',
	kMsgRunAllBench		= 'rnal',
	kMsgSysBenchDone	= 'sbdn',
	kMsgTempUpdate		= 'tmup',
	kMsgExportResults	= 'expr'
};


class MainWindow : public BWindow {
public:
								MainWindow();
	virtual						~MainWindow();

	virtual	void				MessageReceived(BMessage* message);
	virtual	bool				QuitRequested();

private:
			void				_BuildLayout();
			BView*				_CreateSystemInfoPanel();
			BView*				_CreateResultsPanel();

			void				_UpdateTemperature();
			void				_UpdateSysResults();
			void				_ExportResults();

	static	int32				_SysBenchThread(void* data);
	static	int32				_TestCallback(int32 test, void* cookie);

			// System info
			BStringView*		fSystemLabel;
			BStringView*		fHaikuLabel;
			BStringView*		fTempLabel;

			// Sys benchmark results (12 tests)
			BStringView*		fSysTestLabels[SysBenchmark::kNumTests];
			BStringView*		fSysStatusLabel;

			// Score (8 categories + overall)
			BStringView*		fScoreLabels[9];

			// External benchmark results
			BStringView*		fTeapotResultLabel;
			BStringView*		fBench2DResultLabel;
			BStringView*		fGpuResultLabel;
			BStringView*		fVkResultLabel;

			// State
			SysBenchResults		fSysResults;
			BString				fTeapotResult;
			BString				fBench2DResult;
			BString				fGpuResult;
			BString				fVkResult;
			float				fBench2DFPS;	// FillRect ops/sec
			float				fGpuScore;		// GPU overall FPS
			float				fVkScore;		// Vulkan overall pts
			BMessageRunner*		fTempRunner;
			thread_id			fSysBenchThread;
			int32				fRunAllState;	// -1=off, 0-3=step
			volatile int32		fCurrentSysTest;
};


#endif // MAIN_WINDOW_H
