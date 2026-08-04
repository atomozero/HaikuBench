/*
 * Copyright 2025 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef GPU_BENCH_WINDOW_H
#define GPU_BENCH_WINDOW_H


#include <atomic>

#include <Bitmap.h>
#include <GLView.h>
#include <MessageRunner.h>
#include <Messenger.h>
#include <StringView.h>
#include <View.h>


enum {
	kMsgGpuBenchResult	= 'gprs',
	kMsgGpuBenchStart	= 'gpst',
	kMsgGpuRedraw		= 'gprd'
};


static const int32 kNumGpuTests = 6;


struct GpuBenchResults {
	float	fps[kNumGpuTests];
	float	score;
	float	cpuLoad;	// team CPU time / wall time * 100, >100 = multicore
	bool	valid;
};


class GpuBenchGLView : public BGLView {
public:
								GpuBenchGLView(BRect frame);
	virtual						~GpuBenchGLView();

	virtual	void				AttachedToWindow();
	virtual	void				FrameResized(float width, float height);
	virtual	void				Draw(BRect updateRect);

			void				RunBenchmark();
			GpuBenchResults		Results() const { return fResults; }
			BString				GpuInfo() const { return fGpuInfo; }
			BString				RendererName() const { return fRendererName; }
			bool				IsHardware() const;
			int32				CurrentTest() const { return fCurrentTest; }
			bool				IsRunning() const { return fRunning; }

private:
			void				_SetupGL();
			void				_DetectGpu();

			float				_TestFillRate();
			float				_TestGeometry();
			float				_TestTexture();
			float				_TestBlending();
			float				_TestStencil();
			float				_TestCombined();
			void				_SwapWithOverlay();

			GpuBenchResults		fResults;
			BString				fGpuInfo;
			BString				fRendererName;
			int32				fCurrentTest;
			std::atomic<bool>	fRunning;
			float				fCachedWidth;
			float				fCachedHeight;
			bool				fIsZink;
			BBitmap*			fReadbackBitmap;
};


class GpuBenchPanel : public BView {
public:
								GpuBenchPanel(BMessenger target);
	virtual						~GpuBenchPanel();

	virtual	void				AttachedToWindow();
	virtual	void				MessageReceived(BMessage* message);

			// True while a benchmark thread is running or the software
			// comparison pass is in progress (the deck blocks closing while
			// any panel is busy).
			bool				IsBusy() const
									{ return fBenchThread >= 0 || fComparing; }

			// The idle teapot preview is driven by fRedrawRunner; the deck
			// pauses it when the tab is hidden.
			void				StartPreview();
			void				StopPreview();

private:
			void				_UpdateLabels();
			bool				_RunSoftwareComparison();
			void				_SendResults();

	static	int32				_BenchThread(void* data);

			GpuBenchGLView*		fGLView;
			BMessenger			fTarget;

			GpuBenchResults		fSwResults;
			BString				fSwRenderer;
			std::atomic<bool>	fComparing;

			BStringView*		fAccelBadge;
			BStringView*		fCpuLoadLabel;
			BStringView*		fHeaderLabel;
			BStringView*		fGpuLabel;
			BStringView*		fRendererLabel;
			BStringView*		fGlVersionLabel;
			BStringView*		fMesaVersionLabel;
			BStringView*		fTestLabels[kNumGpuTests];
			BStringView*		fStatusLabel;
			BStringView*		fScoreLabel;
			thread_id			fBenchThread;
			BMessageRunner*		fRedrawRunner;
};


// Child-process entry point for the software comparison pass: runs the
// same six tests on the software renderer (launched with HGL_SOFTWARE=1
// from a temporary copy of the binary — the app is B_SINGLE_LAUNCH) and
// writes the results to outputPath.
int RunGpuSoftwarePass(const char* outputPath);


#endif // GPU_BENCH_WINDOW_H
