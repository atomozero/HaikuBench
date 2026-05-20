/*
 * Copyright 2025 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 */


#include "GpuBenchWindow.h"

#include <Box.h>
#include <Button.h>
#include <GL/gl.h>
#include <GroupLayout.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include <LayoutBuilder.h>
#include <SeparatorView.h>

#include "TempOverlay.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static const rgb_color kDarkBg = {13, 17, 23, 255};
static const rgb_color kGreen = {57, 211, 83, 255};
static const rgb_color kWhite = {230, 237, 243, 255};
static const rgb_color kLabelWhite = {200, 210, 225, 255};
static const rgb_color kGray = {170, 180, 195, 255};
static const rgb_color kYellow = {230, 200, 50, 255};

static const float kTestDurationSec = 3.0f;

static const char* kTestNames[kNumGpuTests] = {
	"Fill Rate (flat quads)",
	"Geometry (triangles)",
	"Texture Upload+Render",
	"Alpha Blending",
	"Stencil Buffer",
	"Combined Stress"
};


static BStringView*
_MakeLabel(const char* name, const char* text, const rgb_color& color,
	float fontSize = 11.0f, bool bold = false)
{
	BStringView* view = new BStringView(name, text);
	view->SetViewColor(kDarkBg);
	view->SetLowColor(kDarkBg);
	view->SetHighColor(color);

	BFont font;
	view->GetFont(&font);
	font.SetSize(fontSize);
	if (bold)
		font.SetFace(B_BOLD_FACE);
	view->SetFont(&font);

	return view;
}


// #pragma mark - GpuBenchGLView


GpuBenchGLView::GpuBenchGLView(BRect frame)
	:
	BGLView(frame, "GpuBenchGLView", B_FOLLOW_ALL_SIDES,
		B_WILL_DRAW | B_FRAME_EVENTS,
		BGL_RGB | BGL_DOUBLE | BGL_DEPTH | BGL_STENCIL),
	fCurrentTest(-1),
	fRunning(false),
	fCachedWidth(frame.Width()),
	fCachedHeight(frame.Height()),
	fIsZink(false),
	fReadbackBitmap(NULL)
{
	memset(&fResults, 0, sizeof(fResults));
}


GpuBenchGLView::~GpuBenchGLView()
{
	delete fReadbackBitmap;
}


void
GpuBenchGLView::AttachedToWindow()
{
	BGLView::AttachedToWindow();

	LockGL();
	_SetupGL();
	_DetectGpu();

	const char* renderer = (const char*)glGetString(GL_RENDERER);
	if (renderer != NULL && strstr(renderer, "zink") != NULL) {
		fIsZink = true;
		fprintf(stderr, "HaikuBench: zink detected (%s), using manual "
			"buffer swap workaround\n", renderer);
	}

	UnlockGL();
}


void
GpuBenchGLView::FrameResized(float width, float height)
{
	BGLView::FrameResized(width, height);

	fCachedWidth = width;
	fCachedHeight = height;

	LockGL();
	glViewport(0, 0, (GLsizei)width + 1, (GLsizei)height + 1);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	UnlockGL();
}


void
GpuBenchGLView::Draw(BRect /*updateRect*/)
{
	LockGL();

	// Dark background matching the app theme
	glClearColor(0.051f, 0.067f, 0.09f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (!fRunning) {
		// Idle/complete: show a slowly rotating teapot scene
		float angle = (float)(system_time() / 20000) * 1.0f;

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_LIGHTING);
		glEnable(GL_LIGHT0);
		glEnable(GL_NORMALIZE);
		glDisable(GL_BLEND);
		glDisable(GL_TEXTURE_2D);

		float aspect = (fCachedWidth + 1.0f) / (fCachedHeight + 1.0f);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluPerspective(45.0, aspect, 0.5, 50.0);
		glMatrixMode(GL_MODELVIEW);

		GLfloat lightPos[] = {3.0f, 5.0f, 8.0f, 1.0f};
		glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

		glLoadIdentity();
		glTranslatef(0.0f, -0.3f, -5.0f);
		glRotatef(angle, 0.0f, 1.0f, 0.0f);
		glRotatef(15.0f, 1.0f, 0.0f, 0.0f);

		// Green teapot matching the app accent color
		GLfloat diffuse[] = {0.22f, 0.83f, 0.33f, 1.0f};
		GLfloat specular[] = {1.0f, 1.0f, 1.0f, 1.0f};
		GLfloat shininess[] = {80.0f};
		glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuse);
		glMaterialfv(GL_FRONT, GL_SPECULAR, specular);
		glMaterialfv(GL_FRONT, GL_SHININESS, shininess);

		glutSolidTeapot(1.0);

		// Reset to ortho for overlay
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glDisable(GL_LIGHTING);
		glDisable(GL_DEPTH_TEST);
	}

	_SwapWithOverlay();
	UnlockGL();
}


void
GpuBenchGLView::RunBenchmark()
{
	fRunning = true;
	memset(&fResults, 0, sizeof(fResults));

	LockGL();

	// Use the actual GLView framebuffer size for rendering
	// The viewport matches the real framebuffer pixels

	fCurrentTest = 0;
	fResults.fps[0] = _TestFillRate();

	fCurrentTest = 1;
	fResults.fps[1] = _TestGeometry();

	fCurrentTest = 2;
	fResults.fps[2] = _TestTexture();

	fCurrentTest = 3;
	fResults.fps[3] = _TestBlending();

	fCurrentTest = 4;
	fResults.fps[4] = _TestStencil();

	fCurrentTest = 5;
	fResults.fps[5] = _TestCombined();

	// Calculate overall score (geometric mean)
	float product = 1.0f;
	for (int32 i = 0; i < kNumGpuTests; i++)
		product *= fResults.fps[i] > 0.0f ? fResults.fps[i] : 1.0f;
	fResults.score = powf(product, 1.0f / (float)kNumGpuTests);
	fResults.valid = true;

	UnlockGL();

	fCurrentTest = -1;
	fRunning = false;

	// Trigger a redraw to show results visualization
	if (Window() != NULL && Window()->Lock()) {
		Invalidate();
		Window()->Unlock();
	}
}


void
GpuBenchGLView::_SwapWithOverlay()
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

			// DrawBitmap is a BView call that requires the
			// window lock. Release GL, acquire window lock,
			// draw, release window lock, reacquire GL.
			UnlockGL();
			if (Window()->Lock()) {
				DrawBitmap(fReadbackBitmap, BPoint(0, 0));
				Window()->Unlock();
			}
			LockGL();
		}
	} else {
		glFinish();
		SwapBuffers();
	}
}


void
GpuBenchGLView::_SetupGL()
{
	glViewport(0, 0, (GLsizei)fCachedWidth + 1,
		(GLsizei)fCachedHeight + 1);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}


void
GpuBenchGLView::_DetectGpu()
{
	const char* vendor = (const char*)glGetString(GL_VENDOR);
	const char* renderer = (const char*)glGetString(GL_RENDERER);
	const char* version = (const char*)glGetString(GL_VERSION);

	fGpuInfo.SetToFormat("%s | %s | GL %s",
		vendor ? vendor : "Unknown",
		renderer ? renderer : "Unknown",
		version ? version : "?");
}


float
GpuBenchGLView::_TestFillRate()
{
	// Draw many large overlapping quads to measure fill rate
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);
	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);

	int32 frames = 0;
	bigtime_t start = system_time();
	bigtime_t deadline = start + (bigtime_t)(kTestDurationSec * 1000000.0f);

	while (system_time() < deadline) {
		glClear(GL_COLOR_BUFFER_BIT);

		// Draw 200 full-screen quads with varying colors
		for (int32 i = 0; i < 200; i++) {
			float c = (float)i / 200.0f;
			glColor3f(c, 1.0f - c, 0.5f);
			glBegin(GL_QUADS);
			glVertex2f(-1.0f, -1.0f);
			glVertex2f( 1.0f, -1.0f);
			glVertex2f( 1.0f,  1.0f);
			glVertex2f(-1.0f,  1.0f);
			glEnd();
		}

		glFinish();
		_SwapWithOverlay();
		frames++;
	}

	bigtime_t elapsed = system_time() - start;
	return (float)frames * 1000000.0f / (float)elapsed;
}


float
GpuBenchGLView::_TestGeometry()
{
	// Render many small triangles to test geometry throughput
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);
	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);

	int32 frames = 0;
	bigtime_t start = system_time();
	bigtime_t deadline = start + (bigtime_t)(kTestDurationSec * 1000000.0f);

	while (system_time() < deadline) {
		glClear(GL_COLOR_BUFFER_BIT);

		glBegin(GL_TRIANGLES);
		for (int32 i = 0; i < 10000; i++) {
			float angle = (float)i * 0.1f;
			float r = 0.002f + (float)(i % 50) * 0.001f;
			float cx = sinf(angle * 0.37f) * 0.9f;
			float cy = cosf(angle * 0.53f) * 0.9f;

			float c = (float)(i % 256) / 255.0f;
			glColor3f(c, 1.0f - c, 0.5f * c);
			glVertex2f(cx, cy);
			glVertex2f(cx + r, cy + r);
			glVertex2f(cx - r, cy + r);
		}
		glEnd();

		glFinish();
		_SwapWithOverlay();
		frames++;
	}

	bigtime_t elapsed = system_time() - start;
	return (float)frames * 1000000.0f / (float)elapsed;
}


float
GpuBenchGLView::_TestTexture()
{
	// Create and upload textures, then render textured quads
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);
	glDisable(GL_BLEND);

	// Generate a 256x256 test texture
	const int32 texSize = 256;
	uint8* texData = new uint8[texSize * texSize * 4];
	for (int32 y = 0; y < texSize; y++) {
		for (int32 x = 0; x < texSize; x++) {
			int32 idx = (y * texSize + x) * 4;
			texData[idx + 0] = (uint8)((x ^ y) & 0xFF);
			texData[idx + 1] = (uint8)((x * 3 + y * 7) & 0xFF);
			texData[idx + 2] = (uint8)((x + y * 2) & 0xFF);
			texData[idx + 3] = 255;
		}
	}

	GLuint texId;
	glGenTextures(1, &texId);
	glBindTexture(GL_TEXTURE_2D, texId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	int32 frames = 0;
	bigtime_t start = system_time();
	bigtime_t deadline = start + (bigtime_t)(kTestDurationSec * 1000000.0f);

	while (system_time() < deadline) {
		glClear(GL_COLOR_BUFFER_BIT);

		// Re-upload texture each frame (tests upload bandwidth)
		// Vary the data slightly each frame
		texData[0] = (uint8)(frames & 0xFF);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texSize, texSize, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, texData);

		// Draw multiple textured quads
		glColor3f(1.0f, 1.0f, 1.0f);
		for (int32 i = 0; i < 50; i++) {
			float offset = (float)i * 0.03f;
			glBegin(GL_QUADS);
			glTexCoord2f(0.0f + offset, 0.0f + offset);
			glVertex2f(-0.9f, -0.9f);
			glTexCoord2f(1.0f + offset, 0.0f + offset);
			glVertex2f( 0.9f, -0.9f);
			glTexCoord2f(1.0f + offset, 1.0f + offset);
			glVertex2f( 0.9f,  0.9f);
			glTexCoord2f(0.0f + offset, 1.0f + offset);
			glVertex2f(-0.9f,  0.9f);
			glEnd();
		}

		glFinish();
		_SwapWithOverlay();
		frames++;
	}

	glDeleteTextures(1, &texId);
	glDisable(GL_TEXTURE_2D);
	delete[] texData;

	bigtime_t elapsed = system_time() - start;
	return (float)frames * 1000000.0f / (float)elapsed;
}


float
GpuBenchGLView::_TestBlending()
{
	// Alpha blending stress test
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	int32 frames = 0;
	bigtime_t start = system_time();
	bigtime_t deadline = start + (bigtime_t)(kTestDurationSec * 1000000.0f);

	while (system_time() < deadline) {
		glClear(GL_COLOR_BUFFER_BIT);

		// Draw 300 overlapping semi-transparent quads
		for (int32 i = 0; i < 300; i++) {
			float t = (float)i / 300.0f;
			float size = 0.3f + t * 0.5f;
			float cx = sinf(t * 20.0f + (float)frames * 0.01f) * 0.6f;
			float cy = cosf(t * 15.0f + (float)frames * 0.013f) * 0.6f;

			glColor4f(t, 1.0f - t, 0.5f, 0.15f);
			glBegin(GL_QUADS);
			glVertex2f(cx - size, cy - size);
			glVertex2f(cx + size, cy - size);
			glVertex2f(cx + size, cy + size);
			glVertex2f(cx - size, cy + size);
			glEnd();
		}

		glFinish();
		_SwapWithOverlay();
		frames++;
	}

	glDisable(GL_BLEND);

	bigtime_t elapsed = system_time() - start;
	return (float)frames * 1000000.0f / (float)elapsed;
}


float
GpuBenchGLView::_TestStencil()
{
	// Stencil buffer operations test
	glDisable(GL_LIGHTING);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_STENCIL_TEST);

	int32 frames = 0;
	bigtime_t start = system_time();
	bigtime_t deadline = start + (bigtime_t)(kTestDurationSec * 1000000.0f);

	while (system_time() < deadline) {
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT
			| GL_STENCIL_BUFFER_BIT);

		// Pass 1: Write stencil pattern (circular mask)
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		glStencilFunc(GL_ALWAYS, 1, 0xFF);
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

		// Draw stencil shapes
		for (int32 i = 0; i < 100; i++) {
			float angle = (float)i * 0.063f
				+ (float)frames * 0.02f;
			float cx = sinf(angle) * 0.6f;
			float cy = cosf(angle * 1.3f) * 0.6f;
			float r = 0.15f;

			glBegin(GL_TRIANGLE_FAN);
			glVertex2f(cx, cy);
			for (int32 j = 0; j <= 16; j++) {
				float a = (float)j * 2.0f * M_PI / 16.0f;
				glVertex2f(cx + r * cosf(a), cy + r * sinf(a));
			}
			glEnd();
		}

		// Pass 2: Render only where stencil is set
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glStencilFunc(GL_EQUAL, 1, 0xFF);
		glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

		// Draw colored quads through the stencil
		for (int32 i = 0; i < 150; i++) {
			float t = (float)i / 150.0f;
			glColor3f(t, 0.8f, 1.0f - t);
			glBegin(GL_QUADS);
			glVertex2f(-1.0f, -1.0f + t * 2.0f);
			glVertex2f( 1.0f, -1.0f + t * 2.0f);
			glVertex2f( 1.0f, -0.98f + t * 2.0f);
			glVertex2f(-1.0f, -0.98f + t * 2.0f);
			glEnd();
		}

		glFinish();
		_SwapWithOverlay();
		frames++;
	}

	glDisable(GL_STENCIL_TEST);

	bigtime_t elapsed = system_time() - start;
	return (float)frames * 1000000.0f / (float)elapsed;
}


float
GpuBenchGLView::_TestCombined()
{
	// Combined stress: lighting + depth + texture + blending + geometry
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_NORMALIZE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Setup perspective for 3D
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	float aspect = (fCachedWidth + 1.0f) / (fCachedHeight + 1.0f);
	gluPerspective(60.0, aspect, 0.5, 100.0);
	glMatrixMode(GL_MODELVIEW);

	GLfloat lightPos[] = {3.0f, 5.0f, 8.0f, 1.0f};
	GLfloat lightDiffuse[] = {1.0f, 1.0f, 1.0f, 1.0f};
	glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);

	int32 frames = 0;
	bigtime_t start = system_time();
	bigtime_t deadline = start + (bigtime_t)(kTestDurationSec * 1000000.0f);

	while (system_time() < deadline) {
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		float time = (float)frames * 0.05f;

		// Render a grid of rotating lit objects
		for (int32 i = 0; i < 25; i++) {
			int32 col = i % 5;
			int32 row = i / 5;

			glLoadIdentity();
			glTranslatef(
				-4.0f + col * 2.0f,
				-4.0f + row * 2.0f,
				-12.0f);
			glRotatef(time * 30.0f + (float)i * 15.0f, 0.0f, 1.0f, 0.0f);
			glRotatef(time * 20.0f + (float)i * 23.0f, 1.0f, 0.0f, 0.0f);

			float hue = (float)i / 25.0f;
			GLfloat diffuse[] = {
				0.3f + 0.7f * sinf(hue * 6.28f),
				0.3f + 0.7f * sinf(hue * 6.28f + 2.09f),
				0.3f + 0.7f * sinf(hue * 6.28f + 4.19f),
				0.85f};
			GLfloat specular[] = {1.0f, 1.0f, 1.0f, 1.0f};
			GLfloat shininess[] = {60.0f};

			glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuse);
			glMaterialfv(GL_FRONT, GL_SPECULAR, specular);
			glMaterialfv(GL_FRONT, GL_SHININESS, shininess);

			// Alternate between teapots and spheres
			if (i % 3 == 0)
				glutSolidTeapot(0.6);
			else if (i % 3 == 1)
				glutSolidSphere(0.5, 16, 16);
			else
				glutSolidTorus(0.2, 0.5, 12, 24);
		}

		glFinish();
		_SwapWithOverlay();
		frames++;
	}

	// Restore orthographic projection
	glDisable(GL_LIGHTING);
	glDisable(GL_BLEND);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	bigtime_t elapsed = system_time() - start;
	return (float)frames * 1000000.0f / (float)elapsed;
}


// #pragma mark - GpuBenchWindow


GpuBenchWindow::GpuBenchWindow(BMessenger target)
	:
	BWindow(BRect(120, 120, 780, 620),
		"HaikuBench — GPU Benchmark",
		B_TITLED_WINDOW,
		B_AUTO_UPDATE_SIZE_LIMITS),
	fTarget(target),
	fBenchThread(-1),
	fRedrawRunner(NULL)
{
	BView* topView = new BView("top", B_WILL_DRAW);
	topView->SetViewColor(kDarkBg);

	// GL view — visible preview of the running benchmark
	BRect glFrame(0, 0, 319, 239);
	fGLView = new GpuBenchGLView(glFrame);

	// GPU info panel (BBox)
	BBox* sysBox = new BBox("gpuSysBox");
	sysBox->SetViewColor(kDarkBg);
	sysBox->SetLowColor(kDarkBg);

	BStringView* sysTitle = _MakeLabel("gpuSysTitle", "GPU Device",
		kLabelWhite, 12.0f, true);
	sysTitle->SetViewColor(kDarkBg);
	sysTitle->SetLowColor(kDarkBg);
	sysBox->SetLabel(sysTitle);

	BView* sysInner = new BView("gpuSysInner", B_WILL_DRAW);
	sysInner->SetViewColor(kDarkBg);

	fGpuLabel = _MakeLabel("gpu", "Vendor: detecting...", kWhite, 11.0f, true);
	fRendererLabel = _MakeLabel("renderer", "Renderer: --",
		kWhite, 11.0f);
	fGlVersionLabel = _MakeLabel("glver", "OpenGL: --",
		{80, 200, 220, 255}, 11.0f);
	fMesaVersionLabel = _MakeLabel("mesa", "Mesa: --",
		{80, 200, 220, 255}, 11.0f);

	BLayoutBuilder::Group<>(sysInner, B_VERTICAL, 2)
		.SetInsets(8, 8, 8, 8)
		.Add(fGpuLabel)
		.Add(fRendererLabel)
		.Add(fGlVersionLabel)
		.Add(fMesaVersionLabel)
	.End();
	sysBox->AddChild(sysInner);

	// Results panel (BBox)
	BBox* resBox = new BBox("gpuResBox");
	resBox->SetViewColor(kDarkBg);
	resBox->SetLowColor(kDarkBg);

	BStringView* resTitle = _MakeLabel("gpuResTitle", "Benchmark Results",
		kLabelWhite, 12.0f, true);
	resTitle->SetViewColor(kDarkBg);
	resTitle->SetLowColor(kDarkBg);
	resBox->SetLabel(resTitle);

	BView* resInner = new BView("gpuResInner", B_WILL_DRAW);
	resInner->SetViewColor(kDarkBg);

	BStringView* header = _MakeLabel("gpuHdr", "OpenGL Tests",
		{80, 200, 220, 255}, 11.0f, true);

	BFont monoFont(be_fixed_font);
	monoFont.SetSize(11.0f);

	for (int32 i = 0; i < kNumGpuTests; i++) {
		BString name;
		name.SetToFormat("test%" B_PRId32, i);
		BString text;
		text.SetToFormat("  %-22s       --       ", kTestNames[i]);
		fTestLabels[i] = _MakeLabel(name.String(), text.String(), kGray,
			11.0f);
		fTestLabels[i]->SetFont(&monoFont);
	}

	fScoreLabel = _MakeLabel("score",
		"  Overall Score           --       ", kGreen, 11.0f, true);
	{
		BFont scoreMono(be_fixed_font);
		scoreMono.SetSize(11.0f);
		scoreMono.SetFace(B_BOLD_FACE);
		fScoreLabel->SetFont(&scoreMono);
	}

	BLayoutBuilder::Group<>(resInner, B_VERTICAL, 1)
		.SetInsets(8, 8, 8, 8)
		.Add(header)
		.Add(fTestLabels[0])
		.Add(fTestLabels[1])
		.Add(fTestLabels[2])
		.Add(fTestLabels[3])
		.Add(fTestLabels[4])
		.Add(fTestLabels[5])
		.Add(new BSeparatorView(B_HORIZONTAL))
		.Add(fScoreLabel)
	.End();
	resBox->AddChild(resInner);

	// Status + button
	fStatusLabel = _MakeLabel("status", "Click Start to begin GPU benchmark",
		kGray, 10.0f);

	BButton* startButton = new BButton("startBtn", "Start GPU Benchmark",
		new BMessage(kMsgGpuBenchStart));

	// GLView shows live preview of the running benchmark
	fGLView->SetExplicitMinSize(BSize(320, 240));
	fGLView->SetExplicitMaxSize(BSize(320, 240));

	BLayoutBuilder::Group<>(topView, B_VERTICAL, 6)
		.SetInsets(12, 12, 12, 12)
		.Add(sysBox)
		.Add(fGLView)
		.Add(resBox)
		.AddGlue()
		.Add(new BSeparatorView(B_HORIZONTAL))
		.AddGroup(B_HORIZONTAL, 5)
			.Add(fStatusLabel)
			.AddGlue()
			.Add(startButton)
		.End()
	.End();

	SetLayout(new BGroupLayout(B_VERTICAL));
	AddChild(topView);

	// Timer for teapot animation (~30 FPS)
	BMessage redrawMsg(kMsgGpuRedraw);
	fRedrawRunner = new BMessageRunner(BMessenger(this), &redrawMsg,
		33333);
}


GpuBenchWindow::~GpuBenchWindow()
{
	delete fRedrawRunner;
	if (fBenchThread >= 0) {
		status_t result;
		wait_for_thread(fBenchThread, &result);
	}
}


void
GpuBenchWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgGpuBenchStart:
		{
			if (fBenchThread >= 0)
				break;

			fStatusLabel->SetText("Running GPU benchmark...");
			fStatusLabel->SetHighColor(kYellow);
			fBenchThread = spawn_thread(_BenchThread, "gpu_benchmark",
				B_NORMAL_PRIORITY, this);
			if (fBenchThread >= 0)
				resume_thread(fBenchThread);
			break;
		}

		case kMsgGpuRedraw:
			if (!fGLView->IsRunning())
				fGLView->Invalidate();
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


bool
GpuBenchWindow::QuitRequested()
{
	if (fGLView->IsRunning())
		return false;

	// Send results back if valid
	if (fGLView->Results().valid) {
		GpuBenchResults results = fGLView->Results();
		BMessage result(kMsgGpuBenchResult);
		for (int32 i = 0; i < kNumGpuTests; i++)
			result.AddFloat("gpu_fps", results.fps[i]);
		result.AddFloat("gpu_score", results.score);
		result.AddString("gpu_info", fGLView->GpuInfo());
		fTarget.SendMessage(&result);
	}

	return true;
}


void
GpuBenchWindow::_UpdateLabels()
{
	if (!Lock())
		return;

	GpuBenchResults results = fGLView->Results();
	int32 currentTest = fGLView->CurrentTest();

	// Update GPU info (split into separate lines)
	BString gpuInfo = fGLView->GpuInfo();
	// GpuInfo format: "vendor | renderer | GL version"
	BString vendor, renderer, glVersion;
	int32 sep1 = gpuInfo.FindFirst(" | ");
	if (sep1 >= 0) {
		gpuInfo.CopyInto(vendor, 0, sep1);
		int32 sep2 = gpuInfo.FindFirst(" | ", sep1 + 3);
		if (sep2 >= 0) {
			gpuInfo.CopyInto(renderer, sep1 + 3, sep2 - sep1 - 3);
			gpuInfo.CopyInto(glVersion, sep2 + 3, gpuInfo.Length() - sep2 - 3);
		} else {
			gpuInfo.CopyInto(renderer, sep1 + 3, gpuInfo.Length() - sep1 - 3);
		}
	} else {
		vendor = gpuInfo;
	}

	// Split GL version: "4.5 (Compatibility Profile) Mesa 22.0.5 (...)"
	// into GL part and Mesa part
	BString glPart, mesaPart;
	int32 mesaPos = glVersion.FindFirst("Mesa");
	if (mesaPos > 0) {
		glVersion.CopyInto(glPart, 0, mesaPos);
		glPart.Trim();
		glVersion.CopyInto(mesaPart, mesaPos,
			glVersion.Length() - mesaPos);
	} else {
		glPart = glVersion;
	}

	BString vendorText, rendererText, glText, mesaText;
	vendorText.SetToFormat("Vendor: %s", vendor.String());
	rendererText.SetToFormat("Renderer: %s", renderer.String());
	glText.SetToFormat("OpenGL: %s", glPart.String());
	fGpuLabel->SetText(vendorText.String());
	fRendererLabel->SetText(rendererText.String());
	fGlVersionLabel->SetText(glText.String());
	if (mesaPart.Length() > 0) {
		mesaText.SetToFormat("Driver: %s", mesaPart.String());
		fMesaVersionLabel->SetText(mesaText.String());
	}

	// Update test labels — aligned format matching system bench style
	for (int32 i = 0; i < kNumGpuTests; i++) {
		BString text;
		if (i < currentTest || (currentTest == -1 && results.valid)) {
			text.SetToFormat("  %-22s  %8.1f FPS",
				kTestNames[i], results.fps[i]);
			fTestLabels[i]->SetHighColor(kWhite);
		} else if (i == currentTest) {
			text.SetToFormat("  %-22s  running...",
				kTestNames[i]);
			fTestLabels[i]->SetHighColor(kYellow);
		} else {
			text.SetToFormat("  %-22s       --       ",
				kTestNames[i]);
			fTestLabels[i]->SetHighColor(kGray);
		}
		fTestLabels[i]->SetText(text.String());
	}

	// Update score
	if (results.valid) {
		BString scoreText;
		scoreText.SetToFormat(
			"  Overall Score       %8.1f FPS",
			results.score);
		fScoreLabel->SetText(scoreText.String());
	}

	Unlock();
}


/*static*/ int32
GpuBenchWindow::_BenchThread(void* data)
{
	GpuBenchWindow* window = static_cast<GpuBenchWindow*>(data);

	// Update labels periodically during benchmark
	// The GL view runs the actual benchmark
	thread_id renderThread = spawn_thread(
		[](void* arg) -> int32 {
			GpuBenchGLView* view = static_cast<GpuBenchGLView*>(arg);
			view->RunBenchmark();
			return 0;
		},
		"gpu_render", B_NORMAL_PRIORITY, window->fGLView);

	if (renderThread >= 0)
		resume_thread(renderThread);

	// Poll for progress updates
	while (window->fGLView->IsRunning()) {
		window->_UpdateLabels();
		snooze(250000); // Update UI every 250ms
	}

	// Wait for render thread
	if (renderThread >= 0) {
		status_t result;
		wait_for_thread(renderThread, &result);
	}

	// Final update
	if (window->Lock()) {
		window->_UpdateLabels();
		window->fStatusLabel->SetText("GPU benchmark complete!");
		window->fStatusLabel->SetHighColor(kGreen);
		window->fBenchThread = -1;
		window->Unlock();
	}

	return 0;
}
