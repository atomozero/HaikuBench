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
#include <Window.h>


enum {
	kMsgGpuBenchResult	= 'gprs',
	kMsgGpuBenchStart	= 'gpst',
	kMsgGpuRedraw		= 'gprd'
};


static const int32 kNumGpuTests = 6;


struct GpuBenchResults {
	float	fps[kNumGpuTests];
	float	score;
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
			int32				fCurrentTest;
			std::atomic<bool>	fRunning;
			float				fCachedWidth;
			float				fCachedHeight;
			bool				fIsZink;
			BBitmap*			fReadbackBitmap;
};


class GpuBenchWindow : public BWindow {
public:
								GpuBenchWindow(BMessenger target);
	virtual						~GpuBenchWindow();

	virtual	void				MessageReceived(BMessage* message);
	virtual	bool				QuitRequested();

private:
			void				_UpdateLabels();

	static	int32				_BenchThread(void* data);

			GpuBenchGLView*		fGLView;
			BMessenger			fTarget;

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


#endif // GPU_BENCH_WINDOW_H
