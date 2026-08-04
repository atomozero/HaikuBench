/*
 * Copyright 2025-2026 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef SPLASH_WINDOW_H
#define SPLASH_WINDOW_H


#include <View.h>
#include <Window.h>

#include <vector>

class BBitmap;


// A 64k-intro style splash: a rotating red teapot with the word "HaikuBench"
// orbiting it, over a starfield + metaball backdrop, with a sine-wave scroller
// along the bottom.
//
// Deliberately uses ONLY the app_server 2D API (BView drawing into an offscreen
// BBitmap) — no OpenGL, no Mesa, no glut. The teapot is a tiny software 3D
// renderer (surface-of-revolution mesh, painter's algorithm, flat shading), so
// the splash runs on any Haiku install, including ones with no GL stack at all.
class SplashView : public BView {
public:
							SplashView(BRect frame);
	virtual				~SplashView();

	virtual	void		AttachedToWindow();
	virtual	void		Draw(BRect updateRect);
	virtual	void		MouseDown(BPoint where);
	virtual	void		KeyDown(const char* bytes, int32 numBytes);

			// Render one frame into the offscreen bitmap and blit it to the
			// screen. Returns seconds elapsed since the first frame, so the
			// window can drive the auto-dismiss / fade timeline.
			float		RenderFrame();

private:
			struct Vec3 { float x, y, z; };
			struct Face { int a, b, c, d; };	// quad, indices into fVerts

			void		_BuildTeapotMesh();
			void		_AddRevolution(const float* profile, int count,
							int segments);
			void		_AddTube(const Vec3* path, int count,
							float r0, float r1, int segments);

			void		_DrawBackdrop(BView* c, float t);
			void		_DrawStars(BView* c);
			void		_DrawTeapot(BView* c, float angle, float tilt);
			void		_DrawOrbitTitle(BView* c, float orbit);
			void		_DrawScroller(BView* c, float scroll);
			void		_DrawVignette(BView* c, float fadeAlpha);

	static const int	kNumStars = 200;
			float		fStarX[kNumStars];
			float		fStarY[kNumStars];
			float		fStarZ[kNumStars];

			std::vector<Vec3>	fVerts;
			std::vector<Face>	fFaces;

			BBitmap*	fOffscreen;
			BView*		fCanvas;		// attached to fOffscreen
			float		fWidth;
			float		fHeight;

			bigtime_t	fStartTime;
			float		fAngle;			// teapot spin
			float		fOrbit;			// title orbit angle
			float		fScroll;		// scroller offset (pixels)
};


class SplashWindow : public BWindow {
public:
							// mainWindow is revealed (Show) when the splash
							// finishes or is skipped; the splash then quits.
							SplashWindow(BWindow* mainWindow);
	virtual				~SplashWindow();

	virtual	void		MessageReceived(BMessage* message);
	virtual	bool		QuitRequested();

			// Show a splash unless disabled (env HAIKUBENCH_NOSPLASH), then
			// reveal main when it finishes. If disabled, reveals main
			// immediately and returns NULL.
	static	SplashWindow* Launch(BWindow* mainWindow);

private:
			void		_StartAnimation();
			void		_StopAnimation();
			void		_Dismiss();

	static	int32		_AnimThread(void* data);

			SplashView*		fView;
			BWindow*		fMainWindow;
			thread_id		fAnimThread;
			volatile bool	fRunning;
			bool			fDismissed;
};


#endif // SPLASH_WINDOW_H
