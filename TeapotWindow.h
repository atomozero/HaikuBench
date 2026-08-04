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
#include <View.h>


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


class TeapotPanel : public BView {
public:
								TeapotPanel(BMessenger target);
	virtual						~TeapotPanel();

	virtual	void				AttachedToWindow();
	virtual	void				MessageReceived(BMessage* message);

			// The deck starts/stops rendering when this panel is selected.
			void				StartRendering();
			void				StopRendering();

			// True while the render thread is running (the deck blocks
			// closing while any panel is busy).
			bool				IsBusy() const { return fRendering; }

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
