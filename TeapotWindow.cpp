/*
 * Copyright 2025 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 */


#include "TeapotWindow.h"

#include <Button.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include <GroupLayout.h>
#include <GroupLayoutBuilder.h>
#include <LayoutBuilder.h>
#include <SpaceLayoutItem.h>
#include <StringView.h>

#include "TempOverlay.h"

#include <math.h>
#include <stdio.h>
#include <string.h>


// #pragma mark - TeapotGLView


TeapotGLView::TeapotGLView(BRect frame)
	:
	BGLView(frame, "TeapotGLView", B_FOLLOW_ALL_SIDES,
		B_WILL_DRAW | B_FRAME_EVENTS,
		BGL_RGB | BGL_DOUBLE | BGL_DEPTH),
	fTeapotCount(4),
	fCachedWidth(frame.Width()),
	fCachedHeight(frame.Height()),
	fRotationAngle(0.0f),
	fCurrentFPS(0.0f),
	fFrameCount(0),
	fLastFPSTime(0),
	fIsZink(false),
	fReadbackBitmap(NULL)
{
}


TeapotGLView::~TeapotGLView()
{
	delete fReadbackBitmap;
}


void
TeapotGLView::AttachedToWindow()
{
	BGLView::AttachedToWindow();

	LockGL();
	_SetupGL();
	_SetupLighting();

	const char* renderer = (const char*)glGetString(GL_RENDERER);
	if (renderer != NULL && strstr(renderer, "zink") != NULL) {
		fIsZink = true;
		fprintf(stderr, "HaikuBench: zink detected (%s), using manual "
			"buffer swap workaround\n", renderer);
	}

	UnlockGL();

	fLastFPSTime = system_time();
}


void
TeapotGLView::FrameResized(float width, float height)
{
	BGLView::FrameResized(width, height);

	fCachedWidth = width;
	fCachedHeight = height;

	LockGL();

	glViewport(0, 0, (GLsizei)width + 1, (GLsizei)height + 1);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	float aspect = (width + 1.0f) / (height + 1.0f);
	gluPerspective(45.0, aspect, 0.5, 100.0);
	glMatrixMode(GL_MODELVIEW);

	UnlockGL();
}


void
TeapotGLView::Draw(BRect /*updateRect*/)
{
	Render();
}


void
TeapotGLView::Render()
{
	LockGL();

	glClearColor(0.051f, 0.067f, 0.09f, 1.0f); // #0d1117
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Calculate grid dimensions
	int32 cols = (int32)ceilf(sqrtf((float)fTeapotCount));
	int32 rows = (int32)ceilf((float)fTeapotCount / (float)cols);

	float spacing = 3.5f;
	float startX = -(cols - 1) * spacing / 2.0f;
	float startY = -(rows - 1) * spacing / 2.0f;

	for (int32 i = 0; i < fTeapotCount; i++) {
		int32 col = i % cols;
		int32 row = i / cols;

		glLoadIdentity();
		glTranslatef(
			startX + col * spacing,
			startY + row * spacing,
			-5.0f - rows * 2.0f);

		float phaseOffset = (float)i * 37.0f;
		glRotatef(fRotationAngle + phaseOffset, 0.0f, 1.0f, 0.0f);
		glRotatef(fRotationAngle * 0.7f + phaseOffset * 0.5f, 1.0f, 0.0f,
			0.0f);

		// Shiny red material
		GLfloat ambient[] = {0.15f, 0.02f, 0.02f, 1.0f};
		GLfloat diffuse[] = {0.8f, 0.1f, 0.1f, 1.0f};
		GLfloat specular[] = {1.0f, 0.9f, 0.9f, 1.0f};
		GLfloat shininess[] = {80.0f};

		glMaterialfv(GL_FRONT, GL_AMBIENT, ambient);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuse);
		glMaterialfv(GL_FRONT, GL_SPECULAR, specular);
		glMaterialfv(GL_FRONT, GL_SHININESS, shininess);

		glutSolidTeapot(1.0);
	}

	_SwapWithOverlay();
	UnlockGL();

	fRotationAngle += 1.5f;
	if (fRotationAngle >= 360.0f)
		fRotationAngle -= 360.0f;

	// FPS calculation
	fFrameCount++;
	bigtime_t now = system_time();
	bigtime_t elapsed = now - fLastFPSTime;

	if (elapsed >= 1000000) { // 1 second
		fCurrentFPS = (float)fFrameCount * 1000000.0f / (float)elapsed;
		fFrameCount = 0;
		fLastFPSTime = now;
	}
}


void
TeapotGLView::SetTeapotCount(int32 count)
{
	if (count < 1)
		count = 1;
	if (count > 64)
		count = 64;

	fTeapotCount = count;
}


void
TeapotGLView::_SetupGL()
{
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_NORMALIZE);
	glShadeModel(GL_SMOOTH);

	BRect bounds = Bounds();
	glViewport(0, 0, (GLsizei)bounds.Width() + 1,
		(GLsizei)bounds.Height() + 1);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	float aspect = (bounds.Width() + 1.0f) / (bounds.Height() + 1.0f);
	gluPerspective(45.0, aspect, 0.5, 100.0);
	glMatrixMode(GL_MODELVIEW);
}


void
TeapotGLView::_SwapWithOverlay()
{
	TempOverlay::ReadTemperatures();
	TempOverlay::DrawOverlay(fCachedWidth + 1.0f, fCachedHeight + 1.0f);

	if (fIsZink) {
		// Workaround: BGLView::SwapBuffers() doesn't work with Mesa zink.
		// Manually read back the framebuffer and draw it as a BBitmap.
		glFinish();

		int32 w = (int32)(fCachedWidth + 1);
		int32 h = (int32)(fCachedHeight + 1);
		BRect bounds(0, 0, w - 1, h - 1);

		if (fReadbackBitmap == NULL
			|| fReadbackBitmap->Bounds() != bounds) {
			delete fReadbackBitmap;
			fReadbackBitmap = new BBitmap(bounds, B_RGBA32);
		}

		if (fReadbackBitmap != NULL && fReadbackBitmap->IsValid()) {
			uint8* bits = (uint8*)fReadbackBitmap->Bits();
			int32 bpr = fReadbackBitmap->BytesPerRow();

			// Read each row flipped (GL origin is bottom-left)
			for (int32 y = 0; y < h; y++) {
				glReadPixels(0, h - 1 - y, w, 1,
					GL_BGRA, GL_UNSIGNED_BYTE,
					bits + y * bpr);
			}

			UnlockGL();
			DrawBitmap(fReadbackBitmap, BPoint(0, 0));
			LockGL();
		}
	} else {
		glFinish();
		SwapBuffers();
	}
}


void
TeapotGLView::_SetupLighting()
{
	GLfloat lightPos[] = {5.0f, 5.0f, 10.0f, 1.0f};
	GLfloat lightAmbient[] = {0.15f, 0.15f, 0.15f, 1.0f};
	GLfloat lightDiffuse[] = {1.0f, 1.0f, 1.0f, 1.0f};
	GLfloat lightSpecular[] = {1.0f, 1.0f, 1.0f, 1.0f};

	glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
	glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
	glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular);
}


// #pragma mark - TeapotWindow


TeapotWindow::TeapotWindow(BMessenger target)
	:
	BWindow(BRect(150, 150, 700, 600),
		"HaikuBench — Teapot 3D",
		B_TITLED_WINDOW, 0),
	fTarget(target),
	fCountLabel(NULL),
	fFPSLabel(NULL),
	fRenderThread(-1),
	fRendering(false)
{
	BRect glFrame(0, 0, Bounds().Width(), Bounds().Height() - 80);
	fGLView = new TeapotGLView(glFrame);
	AddChild(fGLView);

	BView* bottomView = new BView(
		BRect(0, Bounds().Height() - 79, Bounds().Width(),
			Bounds().Height()),
		"bottom", B_FOLLOW_LEFT_RIGHT | B_FOLLOW_BOTTOM, B_WILL_DRAW);
	bottomView->SetViewColor(13, 17, 23); // #0d1117

	fCountLabel = new BStringView(
		BRect(10, 5, 200, 22), "count", "Teapots: 4");
	fCountLabel->SetHighColor(230, 237, 243); // #e6edf3
	fCountLabel->SetFont(be_bold_font);
	bottomView->AddChild(fCountLabel);

	fFPSLabel = new BStringView(
		BRect(210, 5, 400, 22), "fps", "FPS: --");
	fFPSLabel->SetHighColor(57, 211, 83); // #39d353
	fFPSLabel->SetFont(be_bold_font);
	bottomView->AddChild(fFPSLabel);

	BButton* addButton = new BButton(
		BRect(10, 28, 70, 50), "add", "+", new BMessage(kMsgAddTeapot));
	bottomView->AddChild(addButton);

	BButton* removeButton = new BButton(
		BRect(80, 28, 140, 50), "remove", "-",
		new BMessage(kMsgRemoveTeapot));
	bottomView->AddChild(removeButton);

	BStringView* tagline = new BStringView(
		BRect(10, 55, Bounds().Width() - 10, 75), "tagline",
		"Haiku has been rendering teapots since 2001. "
		"How many can yours handle?");
	tagline->SetHighColor(170, 180, 195); // kGray — matches palette
	BFont font;
	tagline->GetFont(&font);
	font.SetSize(10.0f);
	tagline->SetFont(&font);
	bottomView->AddChild(tagline);

	AddChild(bottomView);

	_StartRendering();
}


TeapotWindow::~TeapotWindow()
{
	_StopRendering();
}


void
TeapotWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgAddTeapot:
		{
			int32 count = fGLView->TeapotCount();
			if (count < 64) {
				fGLView->SetTeapotCount(count + 1);
				_UpdateCountLabel();
			}
			break;
		}

		case kMsgRemoveTeapot:
		{
			int32 count = fGLView->TeapotCount();
			if (count > 1) {
				fGLView->SetTeapotCount(count - 1);
				_UpdateCountLabel();
			}
			break;
		}

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


bool
TeapotWindow::QuitRequested()
{
	_StopRendering();

	// Send result back to main window
	BMessage result(kMsgTeapotResult);
	result.AddFloat("fps", fGLView->CurrentFPS());
	result.AddInt32("teapots", fGLView->TeapotCount());
	fTarget.SendMessage(&result);

	return true;
}


void
TeapotWindow::_UpdateCountLabel()
{
	BString label;
	label.SetToFormat("Teapots: %" B_PRId32, fGLView->TeapotCount());
	fCountLabel->SetText(label.String());
}


/*static*/ int32
TeapotWindow::_RenderThread(void* data)
{
	TeapotWindow* window = static_cast<TeapotWindow*>(data);

	while (window->fRendering) {
		if (window->LockWithTimeout(50000) == B_OK) {
			window->fGLView->Render();

			BString fpsText;
			fpsText.SetToFormat("FPS: %.1f", window->fGLView->CurrentFPS());
			window->fFPSLabel->SetText(fpsText.String());

			window->Unlock();
		}
		snooze(16667); // ~60 FPS target
	}

	return 0;
}


void
TeapotWindow::_StartRendering()
{
	if (fRendering)
		return;

	fRendering = true;
	fRenderThread = spawn_thread(_RenderThread, "teapot_renderer",
		B_NORMAL_PRIORITY, this);

	if (fRenderThread >= 0)
		resume_thread(fRenderThread);
}


void
TeapotWindow::_StopRendering()
{
	if (!fRendering)
		return;

	fRendering = false;

	if (fRenderThread >= 0) {
		status_t result;
		wait_for_thread(fRenderThread, &result);
		fRenderThread = -1;
	}
}
