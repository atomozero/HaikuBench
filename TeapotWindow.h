/*
 * Copyright 2025 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef TEAPOT_WINDOW_H
#define TEAPOT_WINDOW_H


#include <atomic>

#include <Bitmap.h>
#include <GLView.h>
#include <Messenger.h>
#include <StringView.h>
#include <Window.h>


enum {
	kMsgTeapotResult	= 'tprs',
	kMsgAddTeapot		= 'tpad',
	kMsgRemoveTeapot	= 'tprm'
};


class TeapotGLView : public BGLView {
public:
								TeapotGLView(BRect frame);
	virtual						~TeapotGLView();

	virtual	void				AttachedToWindow();
	virtual	void				FrameResized(float width, float height);
	virtual	void				Draw(BRect updateRect);

			void				Render();
			void				SetTeapotCount(int32 count);
			int32				TeapotCount() const { return fTeapotCount; }
			float				CurrentFPS() const { return fCurrentFPS; }

private:
			void				_SetupGL();
			void				_SetupLighting();
			void				_SwapWithOverlay();

			int32				fTeapotCount;
			float				fCachedWidth;
			float				fCachedHeight;
			float				fRotationAngle;
			float				fCurrentFPS;
			int32				fFrameCount;
			bigtime_t			fLastFPSTime;
			bool				fIsZink;
			BBitmap*			fReadbackBitmap;
};


class TeapotWindow : public BWindow {
public:
								TeapotWindow(BMessenger target);
	virtual						~TeapotWindow();

	virtual	void				MessageReceived(BMessage* message);
	virtual	bool				QuitRequested();

private:
			void				_UpdateCountLabel();
	static	int32				_RenderThread(void* data);
			void				_StartRendering();
			void				_StopRendering();

			BMessenger			fTarget;
			TeapotGLView*		fGLView;
			BStringView*		fCountLabel;
			BStringView*		fFPSLabel;
			thread_id			fRenderThread;
			std::atomic<bool>	fRendering;
};


#endif // TEAPOT_WINDOW_H
