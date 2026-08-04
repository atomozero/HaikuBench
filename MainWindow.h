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

#include "HeaderView.h"
#include "SysBenchmark.h"

class BenchDeckWindow;
class BMenuItem;


enum {
	kMsgRunSysBench		= 'rnsb',
	kMsgToggleSplash	= 'tgsp',
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

			// Scripting (hey) — see the property table in MainWindow.cpp.
	virtual	status_t			GetSupportedSuites(BMessage* message);
	virtual	BHandler*			ResolveSpecifier(BMessage* message,
									int32 index, BMessage* specifier,
									int32 what, const char* property);

private:
			bool				_HandleScripting(BMessage* message);
			float				_OverallScore();

			void				_BuildLayout();
			BView*				_CreateSystemInfoPanel();
			BView*				_CreateResultsPanel();

			void				_UpdateTemperature();
			void				_UpdateSysResults();
			void				_ExportResults();
			void				_SetHeaderStatus(HeaderState state,
									const char* subtitle);
			BenchDeckWindow*	_EnsureDeck();

	static	int32				_SysBenchThread(void* data);
	static	int32				_TestCallback(int32 test, void* cookie);

			// Settings menu item (checkable splash toggle)
			BMenuItem*			fSplashItem;

			// Header banner
			HeaderView*			fHeader;

			// Tabbed benchmark deck (lazily created, persists hidden)
			BenchDeckWindow*	fDeck;

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
