/*
 * Copyright 2025-2026 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 */


#include "SplashWindow.h"

#include <Application.h>
#include <Bitmap.h>
#include <Font.h>
#include <Message.h>
#include <Screen.h>

#include <algorithm>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "Settings.h"
#include "Version.h"


// The whole splash is BView 2D drawing on purpose: it must run on installs with
// no OpenGL/Mesa at all. See SplashWindow.h.

static const float kSplashDuration = 6.0f;		// seconds on screen
static const float kFadeIn = 0.5f;
static const float kFadeOut = 0.8f;

static const uint32 kMsgSplashStart = 'spSt';
static const uint32 kMsgSplashFinish = 'spFi';

static const char* kScrollerText =
	"        HAIKUBENCH " HAIKUBENCH_VERSION
	"   . . .   NATIVE SYSTEM BENCHMARKS FOR A NATIVE OS   . . .   "
	"CPU  MEMORY  CACHE  KERNEL  MESSAGING  2D  OPENGL  VULKAN   . . .   "
	"GREETINGS TO THE HAIKU COMMUNITY   . . .   "
	"NO MESA REQUIRED - THIS INTRO IS 100%% APP_SERVER   . . .   "
	"CLICK OR PRESS A KEY TO CONTINUE   . . .         ";


//	#pragma mark - math helpers


struct V { float x, y, z; };

static inline V
sub(const V& a, const V& b)
{
	V r = {a.x - b.x, a.y - b.y, a.z - b.z};
	return r;
}


static inline V
cross(const V& a, const V& b)
{
	V r = {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x};
	return r;
}


static inline V
normalize(const V& a)
{
	float len = sqrtf(a.x * a.x + a.y * a.y + a.z * a.z);
	if (len < 1e-6f)
		len = 1.0f;
	V r = {a.x / len, a.y / len, a.z / len};
	return r;
}


static inline float
clampf(float v, float lo, float hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}


//	#pragma mark - SplashView


SplashView::SplashView(BRect frame)
	:
	BView(frame, "splash", B_FOLLOW_ALL_SIDES, B_WILL_DRAW),
	fOffscreen(NULL),
	fCanvas(NULL),
	fWidth(frame.Width()),
	fHeight(frame.Height()),
	fStartTime(0),
	fAngle(0.0f),
	fOrbit(0.0f),
	fScroll(0.0f)
{
	SetViewColor(B_TRANSPARENT_COLOR);

	for (int i = 0; i < kNumStars; i++) {
		fStarX[i] = (rand() / (float)RAND_MAX) * 2.0f - 1.0f;
		fStarY[i] = (rand() / (float)RAND_MAX) * 2.0f - 1.0f;
		fStarZ[i] = (rand() / (float)RAND_MAX) * 0.95f + 0.05f;
	}

	_BuildTeapotMesh();
}


SplashView::~SplashView()
{
	delete fOffscreen;	// also deletes its attached fCanvas
}


void
SplashView::AttachedToWindow()
{
	BView::AttachedToWindow();
	MakeFocus(true);

	BRect bounds = Bounds();
	fOffscreen = new BBitmap(bounds, B_RGBA32, true);
	fCanvas = new BView(bounds, "canvas", B_FOLLOW_ALL_SIDES, B_WILL_DRAW);
	fOffscreen->AddChild(fCanvas);
}


void
SplashView::MouseDown(BPoint)
{
	Window()->PostMessage(kMsgSplashFinish);
}


void
SplashView::KeyDown(const char*, int32)
{
	Window()->PostMessage(kMsgSplashFinish);
}


void
SplashView::Draw(BRect)
{
	// Expose repaint: blit the last rendered frame if we have one.
	if (fOffscreen != NULL)
		DrawBitmap(fOffscreen, B_ORIGIN);
}


//	#pragma mark - mesh construction


void
SplashView::_BuildTeapotMesh()
{
	// Teapot body as a surface of revolution: (y, radius) profile from base to
	// the top of the lid knob. Reads as a teapot; the spout and handle are
	// added as swept tubes below.
	static const float kProfile[] = {
		-1.05f, 0.00f,
		-1.05f, 0.55f,
		-0.95f, 0.85f,
		-0.60f, 1.12f,
		-0.20f, 1.22f,
		 0.10f, 1.18f,
		 0.40f, 1.00f,
		 0.62f, 0.72f,
		 0.70f, 0.58f,	// neck
		 0.72f, 0.82f,	// lid overhang rim
		 0.90f, 0.66f,
		 1.06f, 0.30f,	// lid dome
		 1.06f, 0.14f,	// knob base
		 1.20f, 0.20f,	// knob
		 1.32f, 0.00f	// top
	};
	_AddRevolution(kProfile, sizeof(kProfile) / (2 * sizeof(float)), 22);

	// Spout: tapered tube sweeping up and out from the +x side.
	static const Vec3 kSpout[] = {
		{ 0.85f,  0.00f, 0.0f},
		{ 1.35f,  0.15f, 0.0f},
		{ 1.75f,  0.45f, 0.0f},
		{ 1.95f,  0.80f, 0.0f}
	};
	_AddTube(kSpout, 4, 0.30f, 0.09f, 10);

	// Handle: C-curve tube on the -x side.
	static const Vec3 kHandle[] = {
		{-0.95f,  0.55f, 0.0f},
		{-1.55f,  0.48f, 0.0f},
		{-1.75f,  0.05f, 0.0f},
		{-1.62f, -0.42f, 0.0f},
		{-1.10f, -0.60f, 0.0f}
	};
	_AddTube(kHandle, 5, 0.11f, 0.11f, 8);
}


void
SplashView::_AddRevolution(const float* profile, int count, int segments)
{
	int base = (int)fVerts.size();

	for (int p = 0; p < count; p++) {
		float y = profile[p * 2];
		float r = profile[p * 2 + 1];
		for (int s = 0; s < segments; s++) {
			float a = (float)(2.0 * M_PI * s / segments);
			Vec3 v = {r * cosf(a), y, r * sinf(a)};
			fVerts.push_back(v);
		}
	}

	for (int p = 0; p < count - 1; p++) {
		for (int s = 0; s < segments; s++) {
			int s1 = (s + 1) % segments;
			Face f;
			f.a = base + p * segments + s;
			f.b = base + p * segments + s1;
			f.c = base + (p + 1) * segments + s1;
			f.d = base + (p + 1) * segments + s;
			fFaces.push_back(f);
		}
	}
}


void
SplashView::_AddTube(const Vec3* path, int count, float r0, float r1,
	int segments)
{
	int base = (int)fVerts.size();

	for (int i = 0; i < count; i++) {
		// Tangent along the path (central difference).
		V prev = {path[i > 0 ? i - 1 : 0].x, path[i > 0 ? i - 1 : 0].y,
			path[i > 0 ? i - 1 : 0].z};
		V next = {path[i < count - 1 ? i + 1 : count - 1].x,
			path[i < count - 1 ? i + 1 : count - 1].y,
			path[i < count - 1 ? i + 1 : count - 1].z};
		V tangent = normalize(sub(next, prev));

		V up = {0.0f, 1.0f, 0.0f};
		if (fabsf(tangent.x * up.x + tangent.y * up.y + tangent.z * up.z)
				> 0.9f)
			up = (V){1.0f, 0.0f, 0.0f};
		V side = normalize(cross(tangent, up));
		V up2 = normalize(cross(side, tangent));

		float t = count > 1 ? (float)i / (count - 1) : 0.0f;
		float r = r0 + (r1 - r0) * t;

		for (int s = 0; s < segments; s++) {
			float a = (float)(2.0 * M_PI * s / segments);
			float cx = r * cosf(a);
			float cy = r * sinf(a);
			Vec3 v = {
				path[i].x + side.x * cx + up2.x * cy,
				path[i].y + side.y * cx + up2.y * cy,
				path[i].z + side.z * cx + up2.z * cy};
			fVerts.push_back(v);
		}
	}

	for (int i = 0; i < count - 1; i++) {
		for (int s = 0; s < segments; s++) {
			int s1 = (s + 1) % segments;
			Face f;
			f.a = base + i * segments + s;
			f.b = base + i * segments + s1;
			f.c = base + (i + 1) * segments + s1;
			f.d = base + (i + 1) * segments + s;
			fFaces.push_back(f);
		}
	}
}


//	#pragma mark - per-frame rendering


float
SplashView::RenderFrame()
{
	if (fOffscreen == NULL || !fOffscreen->Lock())
		return 0.0f;

	if (fStartTime == 0)
		fStartTime = system_time();
	float t = (float)(system_time() - fStartTime) / 1000000.0f;

	float fade = 0.0f;
	if (t < kFadeIn)
		fade = 1.0f - t / kFadeIn;
	else if (t > kSplashDuration - kFadeOut)
		fade = clampf((t - (kSplashDuration - kFadeOut)) / kFadeOut,
			0.0f, 1.0f);

	_DrawBackdrop(fCanvas, t);
	_DrawStars(fCanvas);
	_DrawOrbitTitle(fCanvas, fOrbit);		// far half (drawn behind teapot)
	_DrawTeapot(fCanvas, fAngle, 0.35f);
	_DrawOrbitTitle(fCanvas, -fOrbit - 1000.0f);	// sentinel: near half
	_DrawScroller(fCanvas, fScroll);
	_DrawVignette(fCanvas, fade * 255.0f);

	fCanvas->Sync();
	fOffscreen->Unlock();

	DrawBitmap(fOffscreen, B_ORIGIN);
	Flush();

	fAngle += 1.8f;
	fOrbit += 0.9f;
	fScroll += 2.6f;

	return t;
}


void
SplashView::_DrawBackdrop(BView* c, float t)
{
	c->SetDrawingMode(B_OP_COPY);
	c->SetHighColor(6, 8, 14);
	c->FillRect(Bounds());

	// Cheap "plasma" — a few large translucent colour blobs drifting.
	c->SetDrawingMode(B_OP_ALPHA);
	c->SetBlendingMode(B_CONSTANT_ALPHA, B_ALPHA_OVERLAY);

	struct Blob { float px, py, rad; rgb_color col; };
	Blob blobs[3] = {
		{0.30f + 0.12f * sinf(t * 0.7f), 0.35f + 0.10f * cosf(t * 0.9f),
			0.55f, {60, 20, 90, 40}},
		{0.70f + 0.10f * cosf(t * 0.6f), 0.55f + 0.12f * sinf(t * 0.5f),
			0.60f, {20, 40, 90, 40}},
		{0.50f + 0.14f * sinf(t * 0.4f), 0.70f + 0.08f * cosf(t * 0.8f),
			0.50f, {90, 20, 40, 36}}
	};
	for (int i = 0; i < 3; i++) {
		float rx = blobs[i].rad * fWidth;
		float ry = blobs[i].rad * fHeight;
		BPoint center(blobs[i].px * fWidth, blobs[i].py * fHeight);
		c->SetHighColor(blobs[i].col);
		c->FillEllipse(center, rx, ry);
	}
}


void
SplashView::_DrawStars(BView* c)
{
	float cx = fWidth * 0.5f;
	float cy = fHeight * 0.5f;
	float scale = fWidth * 0.55f;

	c->SetDrawingMode(B_OP_ALPHA);
	c->SetBlendingMode(B_CONSTANT_ALPHA, B_ALPHA_OVERLAY);

	for (int i = 0; i < kNumStars; i++) {
		fStarZ[i] -= 0.006f;
		if (fStarZ[i] < 0.05f) {
			fStarX[i] = (rand() / (float)RAND_MAX) * 2.0f - 1.0f;
			fStarY[i] = (rand() / (float)RAND_MAX) * 2.0f - 1.0f;
			fStarZ[i] = 1.0f;
		}
		float sx = cx + fStarX[i] / fStarZ[i] * scale;
		float sy = cy + fStarY[i] / fStarZ[i] * scale;
		if (sx < 0 || sx >= fWidth || sy < 0 || sy >= fHeight)
			continue;
		float b = clampf(1.0f - fStarZ[i], 0.0f, 1.0f);
		uint8 a = (uint8)(b * 230.0f);
		float sz = 1.0f + b * 1.8f;
		c->SetHighColor(200, 220, 255, a);
		c->FillRect(BRect(sx, sy, sx + sz, sy + sz));
	}
}


void
SplashView::_DrawTeapot(BView* c, float angle, float tilt)
{
	float cx = fWidth * 0.5f;
	float cy = fHeight * 0.44f;
	float pix = fHeight * 0.20f;
	float camZ = 4.8f;
	float focal = 3.6f;

	// Soft red glow behind the pot.
	c->SetDrawingMode(B_OP_ALPHA);
	c->SetBlendingMode(B_CONSTANT_ALPHA, B_ALPHA_OVERLAY);
	c->SetHighColor(200, 40, 40, 40);
	c->FillEllipse(BPoint(cx, cy), pix * 2.2f, pix * 2.2f);

	float ca = cosf(angle * (float)M_PI / 180.0f);
	float sa = sinf(angle * (float)M_PI / 180.0f);
	float ct = cosf(tilt);
	float st = sinf(tilt);

	int n = (int)fVerts.size();
	std::vector<V> rot(n);
	std::vector<BPoint> proj(n);
	for (int i = 0; i < n; i++) {
		const Vec3& v = fVerts[i];
		// rotate about Y, then tilt about X
		float x1 = ca * v.x + sa * v.z;
		float z1 = -sa * v.x + ca * v.z;
		float y1 = v.y;
		float y2 = ct * y1 - st * z1;
		float z2 = st * y1 + ct * z1;
		rot[i] = (V){x1, y2, z2};

		float d = camZ - z2;
		if (d < 0.1f)
			d = 0.1f;
		float f = focal / d * pix;
		proj[i] = BPoint(cx + x1 * f, cy - y2 * f);
	}

	// Painter's algorithm: sort faces far-to-near (ascending z).
	int fn = (int)fFaces.size();
	std::vector<int> order(fn);
	std::vector<float> depth(fn);
	for (int i = 0; i < fn; i++) {
		const Face& f = fFaces[i];
		depth[i] = (rot[f.a].z + rot[f.b].z + rot[f.c].z + rot[f.d].z)
			* 0.25f;
		order[i] = i;
	}
	std::sort(order.begin(), order.end(),
		[&depth](int p, int q) { return depth[p] < depth[q]; });

	V light = normalize((V){0.35f, 0.55f, 0.75f});

	c->SetDrawingMode(B_OP_COPY);
	for (int oi = 0; oi < fn; oi++) {
		const Face& f = fFaces[order[oi]];
		V nrm = normalize(cross(sub(rot[f.b], rot[f.a]),
			sub(rot[f.d], rot[f.a])));
		float diff = nrm.x * light.x + nrm.y * light.y + nrm.z * light.z;
		if (diff < 0.0f)
			diff = -diff * 0.25f;		// dim back faces, keep them visible
		float shade = clampf(0.18f + 0.82f * diff, 0.0f, 1.0f);

		// specular white glint
		V refl = {2.0f * diff * nrm.x - light.x,
			2.0f * diff * nrm.y - light.y, 2.0f * diff * nrm.z - light.z};
		float spec = clampf(refl.z, 0.0f, 1.0f);
		spec = powf(spec, 18.0f) * 0.9f;

		uint8 r = (uint8)clampf(shade * 225.0f + spec * 180.0f, 0, 255);
		uint8 g = (uint8)clampf(shade * 40.0f + spec * 170.0f, 0, 255);
		uint8 b = (uint8)clampf(shade * 40.0f + spec * 170.0f, 0, 255);

		BPoint pts[4] = {proj[f.a], proj[f.b], proj[f.c], proj[f.d]};
		c->SetHighColor(r, g, b);
		c->FillPolygon(pts, 4);
	}
}


void
SplashView::_DrawOrbitTitle(BView* c, float orbit)
{
	// Called twice per frame. A sentinel value (< -500) selects the near half
	// (drawn after the teapot); otherwise the far half (drawn before it).
	bool nearHalf = orbit < -500.0f;
	if (nearHalf)
		orbit = -(orbit + 1000.0f);

	const char* word = "HaikuBench";
	int len = (int)strlen(word);

	float cx = fWidth * 0.5f;
	float cy = fHeight * 0.44f;
	float rx = fWidth * 0.36f;
	float ry = fHeight * 0.30f;

	c->SetDrawingMode(B_OP_ALPHA);
	c->SetBlendingMode(B_CONSTANT_ALPHA, B_ALPHA_OVERLAY);

	float step = (float)(2.0 * M_PI / len);
	for (int i = 0; i < len; i++) {
		float theta = orbit * (float)M_PI / 180.0f + i * step;
		float s = sinf(theta);
		bool isNear = s < 0.0f;			// lower half = toward viewer
		if (isNear != nearHalf)
			continue;

		float px = cx + rx * cosf(theta);
		float py = cy - ry * s * 0.55f;

		float depth = (-s + 1.0f) * 0.5f;	// 0 far .. 1 near
		float size = fHeight * (0.055f + 0.045f * depth);
		uint8 alpha = (uint8)(90.0f + 165.0f * depth);

		BFont font(be_bold_font);
		font.SetSize(size);
		c->SetFont(&font);

		char ch[2] = {word[i], 0};
		float w = c->StringWidth(ch);

		// Cyan-to-magenta gradient across the word.
		float g = (float)i / (len - 1);
		c->SetHighColor((uint8)(80 + 175 * g), (uint8)(230 - 120 * g),
			(uint8)(230 - 40 * g), alpha);
		c->DrawString(ch, BPoint(px - w * 0.5f, py + size * 0.35f));
	}
}


void
SplashView::_DrawScroller(BView* c, float scroll)
{
	BFont font(be_bold_font);
	font.SetSize(fHeight * 0.055f);
	c->SetFont(&font);

	float baseY = fHeight - fHeight * 0.09f;
	float amp = fHeight * 0.02f;

	c->SetDrawingMode(B_OP_ALPHA);
	c->SetBlendingMode(B_CONSTANT_ALPHA, B_ALPHA_OVERLAY);

	// Total width for looping.
	float total = c->StringWidth(kScrollerText);
	float x = fWidth - fmodf(scroll, total + fWidth);

	int len = (int)strlen(kScrollerText);
	for (int i = 0; i < len; i++) {
		char ch[2] = {kScrollerText[i], 0};
		float w = c->StringWidth(ch);
		if (x > -w && x < fWidth) {
			float y = baseY + sinf(x * 0.018f + scroll * 0.03f) * amp;
			float hue = (float)((i * 7) % 100) / 100.0f;
			c->SetHighColor((uint8)(80 + 120 * hue), (uint8)(220 - 60 * hue),
				(uint8)(120 + 100 * hue), 230);
			c->DrawString(ch, BPoint(x, y));
		}
		x += w;
		if (x > fWidth + 20.0f)
			break;
	}
}


void
SplashView::_DrawVignette(BView* c, float fadeAlpha)
{
	// CRT-ish scanlines.
	c->SetDrawingMode(B_OP_ALPHA);
	c->SetBlendingMode(B_CONSTANT_ALPHA, B_ALPHA_OVERLAY);
	c->SetHighColor(0, 0, 0, 40);
	for (float y = 0; y < fHeight; y += 3.0f)
		c->FillRect(BRect(0, y, fWidth, y));

	// Fade to black at the start / end.
	if (fadeAlpha > 0.5f) {
		c->SetHighColor(0, 0, 0, (uint8)clampf(fadeAlpha, 0, 255));
		c->FillRect(Bounds());
	}
}


//	#pragma mark - SplashWindow


SplashWindow::SplashWindow(BWindow* mainWindow)
	:
	BWindow(BRect(0, 0, 639, 479), "HaikuBench",
		B_NO_BORDER_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
		B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_NOT_MINIMIZABLE
			| B_NOT_CLOSABLE),
	fMainWindow(mainWindow),
	fAnimThread(-1),
	fRunning(false),
	fDismissed(false)
{
	fView = new SplashView(Bounds());
	AddChild(fView);

	// Centre on the main screen.
	BScreen screen;
	BRect sf = screen.Frame();
	MoveTo((sf.Width() - Bounds().Width()) / 2.0f,
		(sf.Height() - Bounds().Height()) / 2.0f);

	// Start animating once the looper is running.
	PostMessage(kMsgSplashStart);
}


SplashWindow::~SplashWindow()
{
	_StopAnimation();
}


/*static*/ SplashWindow*
SplashWindow::Launch(BWindow* mainWindow)
{
	// Disabled by the environment override or by the persisted user setting
	// (Settings menu -> "Show splash screen at startup").
	if (getenv("HAIKUBENCH_NOSPLASH") != NULL || !Settings::ShowSplash()) {
		if (mainWindow != NULL)
			mainWindow->Show();
		return NULL;
	}

	SplashWindow* splash = new SplashWindow(mainWindow);
	splash->Show();
	return splash;
}


void
SplashWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgSplashStart:
			_StartAnimation();
			break;

		case kMsgSplashFinish:
			_Dismiss();
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


bool
SplashWindow::QuitRequested()
{
	_StopAnimation();
	return true;
}


void
SplashWindow::_StartAnimation()
{
	if (fRunning)
		return;
	fRunning = true;
	fAnimThread = spawn_thread(_AnimThread, "splash_anim",
		B_DISPLAY_PRIORITY, this);
	if (fAnimThread >= 0)
		resume_thread(fAnimThread);
	else
		fRunning = false;
}


void
SplashWindow::_StopAnimation()
{
	if (!fRunning && fAnimThread < 0)
		return;
	fRunning = false;
	if (fAnimThread >= 0) {
		status_t result;
		wait_for_thread(fAnimThread, &result);
		fAnimThread = -1;
	}
}


void
SplashWindow::_Dismiss()
{
	if (fDismissed)
		return;
	fDismissed = true;

	_StopAnimation();

	if (fMainWindow != NULL)
		fMainWindow->Show();		// Show() is safe to call cross-thread

	Quit();
}


/*static*/ int32
SplashWindow::_AnimThread(void* data)
{
	SplashWindow* self = static_cast<SplashWindow*>(data);

	while (self->fRunning) {
		float t = 0.0f;
		if (self->LockLooperWithTimeout(20000) == B_OK) {
			t = self->fView->RenderFrame();
			self->UnlockLooper();
		}
		if (t >= kSplashDuration) {
			self->fRunning = false;
			self->PostMessage(kMsgSplashFinish);
			break;
		}
		snooze(16000);		// ~60 FPS target
	}

	return 0;
}
