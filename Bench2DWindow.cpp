/*
 * Copyright 2025 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 */


#include "Bench2DWindow.h"

#include <Bitmap.h>
#include <Box.h>
#include <Button.h>
#include <Font.h>
#include <GroupLayout.h>
#include <GradientLinear.h>
#include <GradientRadial.h>
#include <GradientConic.h>
#include <LayoutBuilder.h>
#include <Polygon.h>
#include <Region.h>
#include <ScrollView.h>
#include <SeparatorView.h>
#include <Shape.h>
#include <Window.h>

#include <Accelerant.h>
#include <Screen.h>
#include <graphic_driver.h>

#include <dirent.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <Catalog.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Bench2D"


static const int32 kBenchW = 800;
static const int32 kBenchH = 600;


// Thread data for parallel pixel computation
struct PlasmaThreadData {
	uint8*		bits;
	int32		bpr;
	int32		startY;
	int32		endY;
	int32		width;
	int32		height;
	float		timeOffset;
};


static int32
_PlasmaWorker(void* data)
{
	PlasmaThreadData* td = static_cast<PlasmaThreadData*>(data);
	float ph = (float)td->height;
	float pw = (float)td->width;

	for (int32 y = td->startY; y < td->endY; y++) {
		float fy = (float)y / ph;
		for (int32 x = 0; x < td->width; x++) {
			float fx = (float)x / pw;
			int32 off = y * td->bpr + x * 4;

			float v1 = sinf(fx * 10.0f + td->timeOffset);
			float v2 = sinf(fy * 8.0f - td->timeOffset * 0.7f);
			float v3 = sinf((fx + fy) * 6.0f
				+ td->timeOffset * 1.3f);
			float v4 = sinf(
				sqrtf((fx - 0.5f) * (fx - 0.5f)
					+ (fy - 0.5f) * (fy - 0.5f))
				* 12.0f - td->timeOffset);
			float v5 = sinf(fx * 15.0f
				* cosf(fy * 7.0f + td->timeOffset));
			float v6 = sinf(fy * 12.0f
				* sinf(fx * 9.0f - td->timeOffset * 0.5f));
			float v7 = cosf(
				sqrtf((fx - 0.3f) * (fx - 0.3f)
					+ (fy - 0.7f) * (fy - 0.7f))
				* 15.0f + td->timeOffset * 1.1f);

			float v = (v1 + v2 + v3 + v4 + v5 + v6 + v7) / 7.0f;

			uint8 r = (uint8)(
				(sinf(v * M_PI) * 0.5f + 0.5f) * 255.0f);
			uint8 g = (uint8)(
				(sinf(v * M_PI + 2.094f) * 0.5f + 0.5f) * 255.0f);
			uint8 b = (uint8)(
				(sinf(v * M_PI + 4.189f) * 0.5f + 0.5f) * 255.0f);

			td->bits[off + 0] = b;
			td->bits[off + 1] = g;
			td->bits[off + 2] = r;
			td->bits[off + 3] = 255;
		}
	}

	return 0;
}


// Thread data for parallel nebula particle pre-computation
struct NebParticle {
	float	cx, cy, rx, ry;
	uint8	r, g, b, alpha;
};


struct NebThreadData {
	NebParticle*	particles;
	int32			start;
	int32			count;
	int32			pass;	// 0=gas, 1=cluster, 2=stars
	uint32			seed;
};


static int32
_NebulaWorker(void* data)
{
	NebThreadData* td = static_cast<NebThreadData*>(data);
	uint32 seed = td->seed;

	// Simple local PRNG to avoid thread contention on rand()
	auto localRand = [&seed]() -> int32 {
		seed = seed * 1103515245 + 12345;
		return (int32)((seed >> 16) & 0x7FFF);
	};

	for (int32 i = 0; i < td->count; i++) {
		NebParticle& p = td->particles[td->start + i];
		p.cx = localRand() % kBenchW;
		p.cy = localRand() % kBenchH;

		if (td->pass == 0) {
			// Gas clouds
			int32 palette = localRand() % 4;
			switch (palette) {
				case 0: p.r = 20 + localRand() % 40;
					p.g = 10 + localRand() % 30;
					p.b = 80 + localRand() % 120; break;
				case 1: p.r = 60 + localRand() % 80;
					p.g = 10 + localRand() % 20;
					p.b = 80 + localRand() % 120; break;
				case 2: p.r = 80 + localRand() % 80;
					p.g = 20 + localRand() % 40;
					p.b = 60 + localRand() % 80; break;
				case 3: p.r = 10 + localRand() % 20;
					p.g = 40 + localRand() % 60;
					p.b = 60 + localRand() % 100; break;
			}
			p.alpha = 8 + localRand() % 25;
			p.rx = (localRand() % 150) + 50;
			p.ry = (localRand() % 150) + 50;
		} else if (td->pass == 1) {
			// Bright clusters
			int32 palette = localRand() % 3;
			switch (palette) {
				case 0: p.r = 100 + localRand() % 100;
					p.g = 50 + localRand() % 100;
					p.b = 150 + localRand() % 105; break;
				case 1: p.r = 150 + localRand() % 105;
					p.g = 80 + localRand() % 80;
					p.b = 120 + localRand() % 135; break;
				case 2: p.r = 50 + localRand() % 60;
					p.g = 100 + localRand() % 100;
					p.b = 160 + localRand() % 95; break;
			}
			p.alpha = 15 + localRand() % 50;
			p.rx = (localRand() % 60) + 10;
			p.ry = (localRand() % 60) + 10;
		} else {
			// Stars
			uint8 brightness = 180 + localRand() % 75;
			if (localRand() % 10 == 0) {
				p.r = brightness;
				p.g = brightness - localRand() % 60;
				p.b = brightness - localRand() % 100;
			} else {
				p.r = p.g = p.b = brightness;
			}
			p.alpha = 120 + localRand() % 135;
			p.rx = p.ry = (localRand() % 4) + 1;

			if (localRand() % 15 == 0)
				p.rx = p.ry = (localRand() % 8) + 4;
		}
	}

	return 0;
}


static uint32
_GetCpuCount()
{
	system_info info;
	if (get_system_info(&info) == B_OK)
		return info.cpu_count;
	return 1;
}


// #pragma mark - Software 3D Teapot Engine


struct Vec3 {
	float x, y, z;

	Vec3() : x(0), y(0), z(0) {}
	Vec3(float ax, float ay, float az) : x(ax), y(ay), z(az) {}

	Vec3 operator+(const Vec3& o) const
		{ return Vec3(x + o.x, y + o.y, z + o.z); }
	Vec3 operator-(const Vec3& o) const
		{ return Vec3(x - o.x, y - o.y, z - o.z); }
	Vec3 operator*(float s) const
		{ return Vec3(x * s, y * s, z * s); }

	float Dot(const Vec3& o) const
		{ return x * o.x + y * o.y + z * o.z; }

	Vec3 Cross(const Vec3& o) const
		{ return Vec3(y * o.z - z * o.y, z * o.x - x * o.z,
			x * o.y - y * o.x); }

	float Length() const { return sqrtf(x * x + y * y + z * z); }

	Vec3 Normalized() const {
		float l = Length();
		if (l < 0.0001f) return Vec3(0, 0, 0);
		return Vec3(x / l, y / l, z / l);
	}
};


struct Tri3D {
	Vec3	v[3];
	Vec3	normal;
	float	depth;
};


static Vec3
_RotateY(Vec3 v, float angle)
{
	float c = cosf(angle), s = sinf(angle);
	return Vec3(v.x * c + v.z * s, v.y, -v.x * s + v.z * c);
}


static Vec3
_RotateX(Vec3 v, float angle)
{
	float c = cosf(angle), s = sinf(angle);
	return Vec3(v.x, v.y * c - v.z * s, v.y * s + v.z * c);
}


static BPoint
_Project(Vec3 v, float projScale, float cx, float cy, float camDist)
{
	// Camera looks down -Z. camDist pushes model away from camera.
	float z = v.z + camDist;
	if (z < 0.5f)
		z = 0.5f;
	float d = projScale / z;
	return BPoint(cx + v.x * d, cy - v.y * d);
}


// Generate a teapot-like surface of revolution + spout + handle
// Returns triangles in model space centered at origin
static void
_GenerateTeapot(Tri3D* tris, int32* triCount, int32 resolution)
{
	*triCount = 0;

	// Body profile (x = radius, y = height), revolved around Y axis
	// Based on simplified Utah teapot silhouette
	const int32 kProfilePoints = 12;
	float profileR[kProfilePoints] = {
		0.0f, 0.6f, 1.0f, 1.35f, 1.5f, 1.5f,
		1.4f, 1.2f, 0.9f, 0.5f, 0.2f, 0.0f
	};
	float profileY[kProfilePoints] = {
		-1.0f, -1.0f, -0.9f, -0.6f, -0.2f, 0.2f,
		0.5f, 0.8f, 1.0f, 1.1f, 1.15f, 1.2f
	};

	int32 nSlices = resolution;
	int32 nStacks = kProfilePoints - 1;

	// Generate body vertices
	Vec3 bodyVerts[64][24]; // [stack+1][slice]
	for (int32 i = 0; i <= nStacks; i++) {
		float r = profileR[i];
		float y = profileY[i];
		for (int32 j = 0; j < nSlices; j++) {
			float angle = (float)j * 2.0f * M_PI / (float)nSlices;
			bodyVerts[i][j] = Vec3(
				r * cosf(angle), y, r * sinf(angle));
		}
	}

	// Body triangles
	for (int32 i = 0; i < nStacks; i++) {
		for (int32 j = 0; j < nSlices; j++) {
			int32 j1 = (j + 1) % nSlices;

			Vec3& a = bodyVerts[i][j];
			Vec3& b = bodyVerts[i][j1];
			Vec3& c = bodyVerts[i + 1][j1];
			Vec3& d = bodyVerts[i + 1][j];

			Vec3 n1 = (b - a).Cross(d - a).Normalized();
			Vec3 n2 = (c - d).Cross(b - d).Normalized();

			Tri3D& t1 = tris[*triCount];
			t1.v[0] = a; t1.v[1] = b; t1.v[2] = d;
			t1.normal = n1;
			(*triCount)++;

			Tri3D& t2 = tris[*triCount];
			t2.v[0] = b; t2.v[1] = c; t2.v[2] = d;
			t2.normal = n2;
			(*triCount)++;
		}
	}

	// Lid: a flattened dome on top
	float lidBaseY = 1.2f;
	float lidTopY = 1.6f;
	float lidR = 0.5f;

	for (int32 i = 0; i < 4; i++) {
		float r0 = lidR * (4 - i) / 4.0f;
		float r1 = lidR * (3 - i) / 4.0f;
		float y0 = lidBaseY
			+ (lidTopY - lidBaseY) * (float)i / 4.0f;
		float y1 = lidBaseY
			+ (lidTopY - lidBaseY) * (float)(i + 1) / 4.0f;

		for (int32 j = 0; j < nSlices; j++) {
			float a0 = (float)j * 2.0f * M_PI / (float)nSlices;
			float a1 = (float)((j + 1) % nSlices) * 2.0f * M_PI
				/ (float)nSlices;

			Vec3 p00(r0 * cosf(a0), y0, r0 * sinf(a0));
			Vec3 p01(r0 * cosf(a1), y0, r0 * sinf(a1));
			Vec3 p10(r1 * cosf(a0), y1, r1 * sinf(a0));
			Vec3 p11(r1 * cosf(a1), y1, r1 * sinf(a1));

			Vec3 n = (p01 - p00).Cross(p10 - p00).Normalized();

			tris[*triCount].v[0] = p00;
			tris[*triCount].v[1] = p01;
			tris[*triCount].v[2] = p10;
			tris[*triCount].normal = n;
			(*triCount)++;

			tris[*triCount].v[0] = p01;
			tris[*triCount].v[1] = p11;
			tris[*triCount].v[2] = p10;
			tris[*triCount].normal = n;
			(*triCount)++;
		}
	}

	// Lid knob (small sphere on top)
	float knobR = 0.15f;
	float knobY = lidTopY + knobR;
	int32 knobRes = 6;

	for (int32 i = 0; i < knobRes; i++) {
		float phi0 = (float)i * M_PI / (float)knobRes;
		float phi1 = (float)(i + 1) * M_PI / (float)knobRes;
		float r0 = knobR * sinf(phi0);
		float r1 = knobR * sinf(phi1);
		float y0 = knobY + knobR * cosf(phi0);
		float y1 = knobY + knobR * cosf(phi1);

		for (int32 j = 0; j < nSlices; j++) {
			float a0 = (float)j * 2.0f * M_PI / (float)nSlices;
			float a1 = (float)((j + 1) % nSlices) * 2.0f * M_PI
				/ (float)nSlices;

			Vec3 p00(r0 * cosf(a0), y0, r0 * sinf(a0));
			Vec3 p01(r0 * cosf(a1), y0, r0 * sinf(a1));
			Vec3 p10(r1 * cosf(a0), y1, r1 * sinf(a0));
			Vec3 p11(r1 * cosf(a1), y1, r1 * sinf(a1));

			Vec3 n = (p01 - p00).Cross(p10 - p00).Normalized();

			tris[*triCount].v[0] = p00;
			tris[*triCount].v[1] = p01;
			tris[*triCount].v[2] = p10;
			tris[*triCount].normal = n;
			(*triCount)++;

			tris[*triCount].v[0] = p01;
			tris[*triCount].v[1] = p11;
			tris[*triCount].v[2] = p10;
			tris[*triCount].normal = n;
			(*triCount)++;
		}
	}

	// Spout: a curved extruded tube
	int32 spoutSegs = 8;
	float spoutR = 0.18f;

	for (int32 i = 0; i < spoutSegs; i++) {
		float t0 = (float)i / (float)spoutSegs;
		float t1 = (float)(i + 1) / (float)spoutSegs;

		// Spout curve: starts at body, curves up and out
		float sx0 = 1.5f + t0 * 1.2f;
		float sy0 = -0.1f + t0 * 1.0f + t0 * t0 * 0.5f;
		float sx1 = 1.5f + t1 * 1.2f;
		float sy1 = -0.1f + t1 * 1.0f + t1 * t1 * 0.5f;

		// Tube radius tapers toward tip
		float tr0 = spoutR * (1.0f - t0 * 0.4f);
		float tr1 = spoutR * (1.0f - t1 * 0.4f);

		for (int32 j = 0; j < 8; j++) {
			float a0 = (float)j * 2.0f * M_PI / 8.0f;
			float a1 = (float)((j + 1) % 8) * 2.0f * M_PI / 8.0f;

			// Direction along spout for orienting the tube cross-section
			Vec3 p00(sx0, sy0 + tr0 * cosf(a0),
				tr0 * sinf(a0));
			Vec3 p01(sx0, sy0 + tr0 * cosf(a1),
				tr0 * sinf(a1));
			Vec3 p10(sx1, sy1 + tr1 * cosf(a0),
				tr1 * sinf(a0));
			Vec3 p11(sx1, sy1 + tr1 * cosf(a1),
				tr1 * sinf(a1));

			Vec3 n = (p01 - p00).Cross(p10 - p00).Normalized();

			tris[*triCount].v[0] = p00;
			tris[*triCount].v[1] = p01;
			tris[*triCount].v[2] = p10;
			tris[*triCount].normal = n;
			(*triCount)++;

			tris[*triCount].v[0] = p01;
			tris[*triCount].v[1] = p11;
			tris[*triCount].v[2] = p10;
			tris[*triCount].normal = n;
			(*triCount)++;
		}
	}

	// Handle: curved tube on opposite side
	int32 handleSegs = 10;
	float handleR = 0.15f;

	for (int32 i = 0; i < handleSegs; i++) {
		float t0 = (float)i / (float)handleSegs;
		float t1 = (float)(i + 1) / (float)handleSegs;

		// Handle curve: a C-shape on the -X side
		float angle0 = -0.3f + t0 * 2.6f;
		float angle1 = -0.3f + t1 * 2.6f;
		float hcx = -1.5f;
		float hcy = 0.3f;
		float hradius = 0.9f;

		float hx0 = hcx + hradius * cosf(angle0) * (-0.6f);
		float hy0 = hcy + hradius * sinf(angle0);
		float hx1 = hcx + hradius * cosf(angle1) * (-0.6f);
		float hy1 = hcy + hradius * sinf(angle1);

		for (int32 j = 0; j < 6; j++) {
			float a0 = (float)j * 2.0f * M_PI / 6.0f;
			float a1 = (float)((j + 1) % 6) * 2.0f * M_PI / 6.0f;

			Vec3 p00(hx0, hy0 + handleR * cosf(a0),
				handleR * sinf(a0));
			Vec3 p01(hx0, hy0 + handleR * cosf(a1),
				handleR * sinf(a1));
			Vec3 p10(hx1, hy1 + handleR * cosf(a0),
				handleR * sinf(a0));
			Vec3 p11(hx1, hy1 + handleR * cosf(a1),
				handleR * sinf(a1));

			Vec3 n = (p01 - p00).Cross(p10 - p00).Normalized();

			tris[*triCount].v[0] = p00;
			tris[*triCount].v[1] = p01;
			tris[*triCount].v[2] = p10;
			tris[*triCount].normal = n;
			(*triCount)++;

			tris[*triCount].v[0] = p01;
			tris[*triCount].v[1] = p11;
			tris[*triCount].v[2] = p10;
			tris[*triCount].normal = n;
			(*triCount)++;
		}
	}

	// Bottom flat disc
	for (int32 j = 0; j < nSlices; j++) {
		float a0 = (float)j * 2.0f * M_PI / (float)nSlices;
		float a1 = (float)((j + 1) % nSlices) * 2.0f * M_PI
			/ (float)nSlices;
		float r = 0.6f;
		float y = -1.0f;

		tris[*triCount].v[0] = Vec3(0, y, 0);
		tris[*triCount].v[1] = Vec3(r * cosf(a1), y, r * sinf(a1));
		tris[*triCount].v[2] = Vec3(r * cosf(a0), y, r * sinf(a0));
		tris[*triCount].normal = Vec3(0, -1, 0);
		(*triCount)++;
	}
}


// Render one teapot to a BView using only 2D primitives
static void
_RenderTeapotSW(BView* view, Tri3D* modelTris, int32 triCount,
	float angleY, float angleX, float projScale, float cx, float cy,
	float camDist, bool wireframe, bool shaded)
{
	// Transform triangles (model is in unit coords, ~3 units wide)
	Tri3D* transformed = new Tri3D[triCount];
	int32 visibleCount = 0;

	Vec3 lightDir = Vec3(0.5f, 0.8f, -0.6f).Normalized();

	for (int32 i = 0; i < triCount; i++) {
		Tri3D t;
		float depth = 0;

		for (int32 v = 0; v < 3; v++) {
			Vec3 p = modelTris[i].v[v];
			p = _RotateX(p, angleX);
			p = _RotateY(p, angleY);
			t.v[v] = p;
			depth += p.z;
		}
		t.depth = depth / 3.0f;

		// Transform normal
		Vec3 n = modelTris[i].normal;
		n = _RotateX(n, angleX);
		n = _RotateY(n, angleY);
		t.normal = n;

		// Backface culling (view direction toward +Z in model space)
		if (t.normal.z > 0.1f)
			continue;

		transformed[visibleCount++] = t;
	}

	// Sort by depth (painter's algorithm) — simple insertion sort
	for (int32 i = 1; i < visibleCount; i++) {
		Tri3D key = transformed[i];
		int32 j = i - 1;
		while (j >= 0 && transformed[j].depth < key.depth) {
			transformed[j + 1] = transformed[j];
			j--;
		}
		transformed[j + 1] = key;
	}

	// Draw
	for (int32 i = 0; i < visibleCount; i++) {
		Tri3D& t = transformed[i];

		BPoint pts[3];
		for (int32 v = 0; v < 3; v++)
			pts[v] = _Project(t.v[v], projScale, cx, cy, camDist);

		if (wireframe) {
			view->SetHighColor(57, 211, 83);
			view->StrokeLine(pts[0], pts[1]);
			view->StrokeLine(pts[1], pts[2]);
			view->StrokeLine(pts[2], pts[0]);
		}

		if (shaded) {
			float intensity = t.normal.Dot(lightDir);
			if (intensity < 0.0f) intensity = 0.0f;

			// Ambient + diffuse, red teapot like the GL version
			uint8 r = (uint8)(30 + 200 * intensity);
			uint8 g = (uint8)(10 + 30 * intensity);
			uint8 b = (uint8)(10 + 30 * intensity);

			// Specular highlight
			Vec3 reflect = t.normal * (2.0f * t.normal.Dot(lightDir))
				- lightDir;
			float spec = reflect.Dot(Vec3(0, 0, -1).Normalized());
			if (spec > 0.0f) {
				spec = powf(spec, 32.0f);
				r = (uint8)fminf(255, r + 200 * spec);
				g = (uint8)fminf(255, g + 180 * spec);
				b = (uint8)fminf(255, b + 180 * spec);
			}

			view->SetHighColor(r, g, b);

			BPolygon polygon(pts, 3);
			view->FillPolygon(&polygon);
		}
	}

	delete[] transformed;
}


// Shared palette — matches MainWindow
static const rgb_color kDarkBg = {13, 17, 23, 255};
static const rgb_color kGreen2D = {57, 211, 83, 255};
static const rgb_color kWhite2D = {230, 237, 243, 255};
static const rgb_color kLabelWhite2D = {200, 210, 225, 255};
static const rgb_color kGray2D = {170, 180, 195, 255};
static const rgb_color kYellow2D = {230, 200, 50, 255};
static const rgb_color kOrange2D = {230, 150, 50, 255};
static const rgb_color kRed2D = {255, 80, 80, 255};
static const rgb_color kMagenta2D = {220, 120, 180, 255};
static const rgb_color kCyan2D = {80, 200, 220, 255};


static const char* kTestNames[kNumBench2DTests] = {
	// Level 1: Basic fills (easy)
	"FillRect (random)",
	"StrokeRect (outlines)",
	"FillRoundRect",
	"StrokeLine (random)",

	// Level 2: Geometry (medium)
	"FillTriangle",
	"FillEllipse",
	"FillPolygon (8-gon)",
	"DrawString (text)",

	// Level 3: Complex compositing (hard)
	"FillBezier (curves)",
	"Gradient fills",
	"Alpha blending",
	"Bitmap scaling",

	// Level 4: Advanced paths (harder)
	"BShape star (24-pt)",
	"Clipped rendering",
	"Conic gradient mesh",
	"Layered text storm",

	// Level 5: Extreme stress (WHAOOOO)
	"Nebula (particles)",
	"Fractal tree",
	"Plasma render",
	"EVERYTHING AT ONCE",

	// Level 6: Software 3D teapot (the boss)
	"Teapot wireframe",
	"Teapot flat-shaded",
	"Teapot x4 rotating",
	"Teapot army (x16)",
};

static const char* kLevelNames[kNumBench2DLevels] = {
	"LEVEL 1 \xE2\x80\x94 Basic Fills",
	"LEVEL 2 \xE2\x80\x94 Geometry",
	"LEVEL 3 \xE2\x80\x94 Complex Compositing",
	"LEVEL 4 \xE2\x80\x94 Advanced Paths",
	"LEVEL 5 \xE2\x80\x94 EXTREME STRESS",
	"LEVEL 6 \xE2\x80\x94 SOFTWARE 3D TEAPOT",
};

static const rgb_color kLevelColors[kNumBench2DLevels] = {
	{57, 211, 83, 255},		// green
	{230, 150, 50, 255},	// orange (matches MainWindow)
	{255, 80, 80, 255},		// red
	{220, 120, 180, 255},	// magenta (matches MainWindow)
	{80, 200, 220, 255},	// cyan (matches MainWindow)
	{230, 200, 50, 255},	// yellow/gold (matches MainWindow)
};

static const int32 kTestOps[kNumBench2DTests] = {
	// Level 1
	5000, 5000, 3000, 10000,
	// Level 2
	3000, 2000, 1500, 3000,
	// Level 3
	1000, 500, 2000, 500,
	// Level 4
	800, 300, 200, 500,
	// Level 5 (heavy: ~10s each)
	15000, 50, 20, 10,
	// Level 6 (software 3D teapot)
	60, 30, 20, 8,
};


// #pragma mark - Bench2DView


Bench2DView::Bench2DView()
	:
	BView(BRect(0, 0, kBenchW - 1, kBenchH - 1),
		"bench2d", B_FOLLOW_ALL, B_WILL_DRAW),
	fDriverInfo("Detecting...")
{
	memset(&fResults, 0, sizeof(fResults));
}


void
Bench2DView::AttachedToWindow()
{
	SetViewColor(0, 0, 0);
}


void
Bench2DView::Draw(BRect /*updateRect*/)
{
}


void
Bench2DView::_DetectDriver()
{
	fDriverInfo = "Unknown";

	FILE* fp = popen("listimage 2>/dev/null | grep accelerant", "r");
	if (fp != NULL) {
		char line[512];
		while (fgets(line, sizeof(line), fp) != NULL) {
			char* nl = strchr(line, '\n');
			if (nl != NULL)
				*nl = '\0';

			if (strstr(line, "non-packaged") != NULL)
				fDriverInfo = "PATCHED (non-packaged)";
			else if (strstr(line, "accelerant") != NULL) {
				char* lastSlash = strrchr(line, '/');
				if (lastSlash != NULL)
					fDriverInfo.SetToFormat("System: %s", lastSlash + 1);
				else
					fDriverInfo = "System (package)";
			}
		}
		pclose(fp);
	}

	// Detect 2D hardware acceleration via accelerant clone
	_DetectAcceleration();
}


void
Bench2DView::_DetectAcceleration()
{
	fAccelInfo.accelerantName = "";
	fAccelInfo.deviceName = "";
	fAccelInfo.chipset = "";
	fAccelInfo.vramBytes = 0;
	fAccelInfo.isHardwareAccel = false;
	fAccelInfo.hasFillRect = false;
	fAccelInfo.hasScreenBlit = false;
	fAccelInfo.hasInvertRect = false;
	fAccelInfo.hooksChecked = false;

	// Step 1: Get device info via BScreen (public API)
	BScreen screen;
	accelerant_device_info devInfo;
	memset(&devInfo, 0, sizeof(devInfo));
	if (screen.GetDeviceInfo(&devInfo) == B_OK) {
		fAccelInfo.deviceName = devInfo.name;
		fAccelInfo.chipset = devInfo.chipset;
		fAccelInfo.vramBytes = devInfo.memory;
	}

	// Step 2: Get accelerant signature from the graphics device
	DIR* dir = opendir("/dev/graphics");
	if (dir != NULL) {
		BString devicePath;
		BString fallbackPath;
		struct dirent* entry;
		while ((entry = readdir(dir)) != NULL) {
			if (strcmp(entry->d_name, ".") == 0
				|| strcmp(entry->d_name, "..") == 0)
				continue;
			BString path;
			path.SetToFormat("/dev/graphics/%s", entry->d_name);
			if (strcmp(entry->d_name, "vesa") == 0)
				fallbackPath = path;
			else
				devicePath = path;
		}
		closedir(dir);

		if (devicePath.Length() == 0)
			devicePath = fallbackPath;

		if (devicePath.Length() > 0) {
			int fd = open(devicePath.String(), O_RDWR);
			if (fd >= 0) {
				char signature[1024] = {};
				if (ioctl(fd, B_GET_ACCELERANT_SIGNATURE, signature,
						sizeof(signature)) == B_OK) {
					fAccelInfo.accelerantName = signature;
				}
				close(fd);
			}
		}
	}

	// Step 3: Determine 2D acceleration from the accelerant name.
	// Haiku's accelerant drivers are well-known: native GPU drivers
	// implement 2D hooks (fill_rectangle, screen_to_screen_blit),
	// while generic drivers (vesa, framebuffer) do not.
	BString name = fAccelInfo.accelerantName;
	if (name.Length() == 0)
		name = fDriverInfo;
	name.ToLower();

	// Known software-only drivers
	static const char* kSoftwareOnly[] = {
		"vesa", "framebuffer", "virtio_gpu", NULL
	};

	// Known hardware-accelerated drivers (with 2D hooks)
	static const char* kHwAccelerated[] = {
		"intel_extreme", "intel_810", "radeon_hd", "radeon",
		"nvidia", "ati", "matrox", "via", "3dfx", "s3", "neomagic",
		NULL
	};

	bool matched = false;

	for (int32 i = 0; kSoftwareOnly[i] != NULL; i++) {
		if (name.FindFirst(kSoftwareOnly[i]) >= 0) {
			fAccelInfo.isHardwareAccel = false;
			fAccelInfo.hasFillRect = false;
			fAccelInfo.hasScreenBlit = false;
			fAccelInfo.hasInvertRect = false;
			fAccelInfo.hooksChecked = true;
			matched = true;
			break;
		}
	}

	if (!matched) {
		for (int32 i = 0; kHwAccelerated[i] != NULL; i++) {
			if (name.FindFirst(kHwAccelerated[i]) >= 0) {
				fAccelInfo.isHardwareAccel = true;
				fAccelInfo.hasFillRect = true;
				fAccelInfo.hasScreenBlit = true;
				fAccelInfo.hasInvertRect = true;
				fAccelInfo.hooksChecked = true;
				matched = true;
				break;
			}
		}
	}

	// Unknown driver: assume software only
	if (!matched && name.Length() > 0) {
		fAccelInfo.isHardwareAccel = false;
		fAccelInfo.hooksChecked = false;
	}

	fprintf(stderr, "HaikuBench: 2D accel detection: hw=%d driver=%s "
		"device=%s chipset=%s vram=%u\n",
		fAccelInfo.isHardwareAccel, name.String(),
		fAccelInfo.deviceName.String(),
		fAccelInfo.chipset.String(),
		(unsigned)fAccelInfo.vramBytes);
}


void
Bench2DView::RunBenchmark()
{
	_DetectDriver();

	// Warm up
	BRect bounds = Bounds();
	for (int i = 0; i < 100; i++) {
		SetHighColor(0, 0, 0);
		FillRect(bounds);
	}
	Sync();

	fResults.valid = true;

	for (int32 t = 0; t < kNumBench2DTests; t++)
		_RunTest(t);

	// Clear to dark after benchmark
	SetHighColor(13, 17, 23);
	FillRect(bounds);
	Sync();
}


void
Bench2DView::_RunTest(int32 testIndex)
{
	int32 numOps = kTestOps[testIndex];
	bigtime_t start, elapsed;

	srand(12345 + testIndex * 7919);

	start = system_time();

	switch (testIndex) {

		// ==== LEVEL 1: Basic Fills ====

		case 0: // FillRect
		{
			for (int32 i = 0; i < numOps; i++) {
				SetHighColor(rand() % 256, rand() % 256, rand() % 256);
				float x1 = rand() % kBenchW;
				float y1 = rand() % kBenchH;
				float x2 = x1 + (rand() % 200) + 10;
				float y2 = y1 + (rand() % 200) + 10;
				FillRect(BRect(x1, y1, x2, y2));
			}
			break;
		}

		case 1: // StrokeRect
		{
			for (int32 i = 0; i < numOps; i++) {
				SetHighColor(rand() % 256, rand() % 256, rand() % 256);
				SetPenSize((float)(rand() % 4) + 1.0f);
				float x1 = rand() % kBenchW;
				float y1 = rand() % kBenchH;
				float x2 = x1 + (rand() % 200) + 20;
				float y2 = y1 + (rand() % 200) + 20;
				StrokeRect(BRect(x1, y1, x2, y2));
			}
			SetPenSize(1.0f);
			break;
		}

		case 2: // FillRoundRect
		{
			for (int32 i = 0; i < numOps; i++) {
				SetHighColor(rand() % 256, rand() % 256, rand() % 256);
				float x1 = rand() % kBenchW;
				float y1 = rand() % kBenchH;
				float w = (rand() % 200) + 30;
				float h = (rand() % 150) + 30;
				float r = (rand() % 20) + 5;
				FillRoundRect(BRect(x1, y1, x1 + w, y1 + h), r, r);
			}
			break;
		}

		case 3: // StrokeLine
		{
			for (int32 i = 0; i < numOps; i++) {
				SetHighColor(rand() % 256, rand() % 256, rand() % 256);
				SetPenSize((float)(rand() % 5) + 1.0f);
				BPoint from(rand() % kBenchW, rand() % kBenchH);
				BPoint to(rand() % kBenchW, rand() % kBenchH);
				StrokeLine(from, to);
			}
			SetPenSize(1.0f);
			break;
		}

		// ==== LEVEL 2: Geometry ====

		case 4: // FillTriangle
		{
			for (int32 i = 0; i < numOps; i++) {
				SetHighColor(rand() % 256, rand() % 256, rand() % 256);
				BPoint p1(rand() % kBenchW, rand() % kBenchH);
				BPoint p2(p1.x + (rand() % 200) - 100,
					p1.y + (rand() % 200) - 100);
				BPoint p3(p1.x + (rand() % 200) - 100,
					p1.y + (rand() % 200) - 100);
				FillTriangle(p1, p2, p3);
			}
			break;
		}

		case 5: // FillEllipse
		{
			for (int32 i = 0; i < numOps; i++) {
				SetHighColor(rand() % 256, rand() % 256, rand() % 256);
				BPoint center(rand() % kBenchW, rand() % kBenchH);
				float rx = (rand() % 100) + 10;
				float ry = (rand() % 100) + 10;
				FillEllipse(center, rx, ry);
			}
			break;
		}

		case 6: // FillPolygon (octagon)
		{
			for (int32 i = 0; i < numOps; i++) {
				SetHighColor(rand() % 256, rand() % 256, rand() % 256);
				float cx = rand() % kBenchW;
				float cy = rand() % kBenchH;
				float radius = (rand() % 80) + 20;

				BPoint points[8];
				for (int32 s = 0; s < 8; s++) {
					float angle = (float)s * 2.0f * M_PI / 8.0f;
					points[s].x = cx + radius * cosf(angle);
					points[s].y = cy + radius * sinf(angle);
				}

				BPolygon polygon(points, 8);
				FillPolygon(&polygon);
			}
			break;
		}

		case 7: // DrawString
		{
			const char* texts[] = {
				"Haiku OS", "HaikuBench",
				"Saving the planet!", "2D acceleration test",
				"BeOS lives on", "The efficient OS",
			};

			for (int32 i = 0; i < numOps; i++) {
				SetHighColor(rand() % 256, rand() % 256, rand() % 256);
				BFont font(be_plain_font);
				font.SetSize((float)(rand() % 36) + 8.0f);
				SetFont(&font);
				BPoint pos(rand() % (kBenchW - 100), rand() % kBenchH);
				DrawString(texts[i % 6], pos);
			}
			break;
		}

		// ==== LEVEL 3: Complex Compositing ====

		case 8: // FillBezier
		{
			for (int32 i = 0; i < numOps; i++) {
				SetHighColor(rand() % 256, rand() % 256, rand() % 256);
				BPoint cp[4];
				for (int32 j = 0; j < 4; j++)
					cp[j].Set(rand() % kBenchW, rand() % kBenchH);
				FillBezier(cp);
			}
			break;
		}

		case 9: // Gradient fills (linear + radial)
		{
			for (int32 i = 0; i < numOps; i++) {
				float x1 = rand() % kBenchW;
				float y1 = rand() % kBenchH;
				float w = (rand() % 200) + 50;
				float h = (rand() % 200) + 50;
				BRect rect(x1, y1, x1 + w, y1 + h);

				if (i % 2 == 0) {
					BGradientLinear gradient(
						BPoint(x1, y1), BPoint(x1 + w, y1 + h));
					gradient.AddColor((rgb_color){
						(uint8)(rand() % 256), (uint8)(rand() % 256),
						(uint8)(rand() % 256), 255}, 0.0f);
					gradient.AddColor((rgb_color){
						(uint8)(rand() % 256), (uint8)(rand() % 256),
						(uint8)(rand() % 256), 255}, 255.0f);
					FillRect(rect, gradient);
				} else {
					BGradientRadial gradient(
						BPoint(x1 + w / 2, y1 + h / 2),
						w > h ? w / 2 : h / 2);
					gradient.AddColor((rgb_color){
						(uint8)(rand() % 256), (uint8)(rand() % 256),
						(uint8)(rand() % 256), 255}, 0.0f);
					gradient.AddColor((rgb_color){
						(uint8)(rand() % 256), (uint8)(rand() % 256),
						(uint8)(rand() % 256), 128}, 255.0f);
					FillEllipse(rect, gradient);
				}
			}
			break;
		}

		case 10: // Alpha blending
		{
			SetDrawingMode(B_OP_ALPHA);
			for (int32 i = 0; i < numOps; i++) {
				uint8 alpha = (rand() % 200) + 30;
				SetHighColor(rand() % 256, rand() % 256,
					rand() % 256, alpha);
				float cx = rand() % kBenchW;
				float cy = rand() % kBenchH;
				float r = (rand() % 100) + 20;
				FillEllipse(BPoint(cx, cy), r, r * 0.7f);
			}
			SetDrawingMode(B_OP_COPY);
			break;
		}

		case 11: // Bitmap scaling
		{
			int32 bmpSize = 64;
			BBitmap* bitmap = new BBitmap(
				BRect(0, 0, bmpSize - 1, bmpSize - 1), B_RGBA32);

			if (bitmap->InitCheck() == B_OK) {
				uint8* bits = (uint8*)bitmap->Bits();
				int32 bpr = bitmap->BytesPerRow();
				for (int32 y = 0; y < bmpSize; y++) {
					for (int32 x = 0; x < bmpSize; x++) {
						int32 off = y * bpr + x * 4;
						bool check = ((x / 8) + (y / 8)) % 2 == 0;
						bits[off + 0] = check ? 200 : 50;
						bits[off + 1] = (uint8)(y * 4);
						bits[off + 2] = (uint8)(x * 4);
						bits[off + 3] = 255;
					}
				}

				for (int32 i = 0; i < numOps; i++) {
					float dx = rand() % kBenchW;
					float dy = rand() % kBenchH;
					float scale = (float)(rand() % 300 + 50) / 100.0f;
					float dw = bmpSize * scale;
					float dh = bmpSize * scale;
					DrawBitmap(bitmap,
						BRect(0, 0, bmpSize - 1, bmpSize - 1),
						BRect(dx, dy, dx + dw, dy + dh));
				}
				delete bitmap;
			}
			break;
		}

		// ==== LEVEL 4: Advanced Paths ====

		case 12: // BShape star (24-point stars via BShape)
		{
			for (int32 i = 0; i < numOps; i++) {
				SetHighColor(rand() % 256, rand() % 256, rand() % 256);

				float cx = rand() % kBenchW;
				float cy = rand() % kBenchH;
				float outerR = (rand() % 60) + 30;
				float innerR = outerR * 0.4f;
				int32 points = 24;

				BShape shape;
				for (int32 p = 0; p < points * 2; p++) {
					float angle = (float)p * M_PI / (float)points
						- M_PI / 2.0f;
					float r = (p % 2 == 0) ? outerR : innerR;
					float px = cx + r * cosf(angle);
					float py = cy + r * sinf(angle);

					if (p == 0)
						shape.MoveTo(BPoint(px, py));
					else
						shape.LineTo(BPoint(px, py));
				}
				shape.Close();
				FillShape(&shape);
			}
			break;
		}

		case 13: // Clipped rendering (complex clip region + draw)
		{
			for (int32 i = 0; i < numOps; i++) {
				// Create a complex clip region with multiple rects
				BRegion clipRegion;
				int32 numClipRects = 20;
				for (int32 c = 0; c < numClipRects; c++) {
					float rx = rand() % kBenchW;
					float ry = rand() % kBenchH;
					float rw = (rand() % 100) + 20;
					float rh = (rand() % 100) + 20;
					clipRegion.Include(
						BRect(rx, ry, rx + rw, ry + rh));
				}

				ConstrainClippingRegion(&clipRegion);

				// Draw many shapes through the clip
				for (int32 d = 0; d < 30; d++) {
					SetHighColor(rand() % 256, rand() % 256,
						rand() % 256);
					float ex = rand() % kBenchW;
					float ey = rand() % kBenchH;
					FillEllipse(BPoint(ex, ey),
						(rand() % 80) + 20, (rand() % 80) + 20);
				}

				ConstrainClippingRegion(NULL);
			}
			break;
		}

		case 14: // Conic gradient mesh
		{
			for (int32 i = 0; i < numOps; i++) {
				float cx = rand() % kBenchW;
				float cy = rand() % kBenchH;
				float r = (rand() % 120) + 60;

				BGradientConic gradient(BPoint(cx, cy), 0.0f);

				// Multi-stop rainbow gradient
				gradient.AddColor((rgb_color){255, 0, 0, 255}, 0.0f);
				gradient.AddColor((rgb_color){255, 255, 0, 255}, 42.0f);
				gradient.AddColor((rgb_color){0, 255, 0, 255}, 85.0f);
				gradient.AddColor((rgb_color){0, 255, 255, 255}, 128.0f);
				gradient.AddColor((rgb_color){0, 0, 255, 255}, 170.0f);
				gradient.AddColor((rgb_color){255, 0, 255, 255}, 212.0f);
				gradient.AddColor((rgb_color){255, 0, 0, 255}, 255.0f);

				FillEllipse(BPoint(cx, cy), r, r, gradient);
			}
			break;
		}

		case 15: // Layered text storm (many fonts, sizes, alpha, overlapping)
		{
			const char* words[] = {
				"HAIKU", "WATTS", "GREEN", "FAST", "LEAN",
				"POWER", "SAVE", "TREE", "CO2", "EARTH",
				"BENCH", "PIXEL", "DRAW", "FILL", "BLIT",
			};

			SetDrawingMode(B_OP_ALPHA);

			for (int32 i = 0; i < numOps; i++) {
				uint8 alpha = (rand() % 180) + 40;
				SetHighColor(rand() % 256, rand() % 256,
					rand() % 256, alpha);

				BFont font;
				float sizes[] = {10, 14, 20, 28, 40, 56, 72, 96};
				font.SetSize(sizes[rand() % 8]);

				int32 face = rand() % 3;
				if (face == 0) font.SetFace(B_BOLD_FACE);
				else if (face == 1) font.SetFace(B_ITALIC_FACE);
				else font.SetFace(B_REGULAR_FACE);

				// Slight rotation via shear
				font.SetShear((float)(rand() % 60) + 60.0f);

				SetFont(&font);
				BPoint pos(rand() % kBenchW, rand() % kBenchH);
				DrawString(words[rand() % 15], pos);
			}

			SetDrawingMode(B_OP_COPY);
			break;
		}

		// ==== LEVEL 5: EXTREME STRESS ====

		case 16: // Nebula — particle field, parallel pre-computation
		{
			uint32 cpuCount = _GetCpuCount();

			// Pre-compute ALL particles in parallel
			int32 perPass = numOps / 3;
			int32 totalParticles = numOps;
			NebParticle* particles = new NebParticle[totalParticles];

			// Launch threads for each pass
			for (int32 pass = 0; pass < 3; pass++) {
				int32 passStart = pass * perPass;
				int32 passCount = (pass < 2) ? perPass
					: (totalParticles - 2 * perPass);

				int32 threadsToUse = (int32)cpuCount;
				if (threadsToUse > passCount)
					threadsToUse = passCount;

				thread_id* threads = new thread_id[threadsToUse];
				NebThreadData* tdata = new NebThreadData[threadsToUse];

				int32 chunkSize = passCount / threadsToUse;

				for (int32 t = 0; t < threadsToUse; t++) {
					tdata[t].particles = particles;
					tdata[t].start = passStart + t * chunkSize;
					tdata[t].count = (t == threadsToUse - 1)
						? (passCount - t * chunkSize) : chunkSize;
					tdata[t].pass = pass;
					tdata[t].seed = 42 + pass * 9973 + t * 7919;

					threads[t] = spawn_thread(_NebulaWorker,
						"nebula_worker", B_NORMAL_PRIORITY,
						&tdata[t]);
					if (threads[t] >= 0)
						resume_thread(threads[t]);
				}

				for (int32 t = 0; t < threadsToUse; t++) {
					status_t result;
					wait_for_thread(threads[t], &result);
				}

				delete[] threads;
				delete[] tdata;
			}

			// Now draw all particles (must be on window thread)
			SetHighColor(5, 5, 15);
			FillRect(Bounds());
			SetDrawingMode(B_OP_ALPHA);

			for (int32 i = 0; i < totalParticles; i++) {
				NebParticle& p = particles[i];
				SetHighColor(p.r, p.g, p.b, p.alpha);

				// Stars with glow halos
				int32 pass = (i < perPass) ? 0
					: (i < 2 * perPass) ? 1 : 2;
				if (pass == 2 && p.rx >= 4) {
					// Draw glow halo first
					SetHighColor(p.r / 2, p.g / 2, p.b,
						p.alpha / 3);
					FillEllipse(BPoint(p.cx, p.cy),
						p.rx * 3, p.ry * 3);
					SetHighColor(p.r, p.g, p.b, p.alpha);
				}

				FillEllipse(BPoint(p.cx, p.cy), p.rx, p.ry);
			}

			SetDrawingMode(B_OP_COPY);
			delete[] particles;
			break;
		}

		case 17: // Fractal tree — recursive branching
		{
			struct Branch {
				float x, y, angle, length;
				int32 depth;
			};

			for (int32 t = 0; t < numOps; t++) {
				// Clear
				SetHighColor(5, 5, 15);
				FillRect(Bounds());

				// Stack-based recursive tree
				Branch stack[4096];
				int32 stackSize = 0;

				float baseX = kBenchW / 2 + (rand() % 200 - 100);
				float treeAngle = -M_PI / 2.0f
					+ (float)(rand() % 20 - 10) * M_PI / 180.0f;

				stack[stackSize++] = {baseX, (float)kBenchH,
					treeAngle, 120.0f + rand() % 40, 0};

				int32 maxDepth = 11;
				int32 branchCount = 0;

				while (stackSize > 0 && branchCount < 4000) {
					Branch b = stack[--stackSize];
					branchCount++;

					if (b.depth >= maxDepth || b.length < 2.0f)
						continue;

					float endX = b.x + b.length * cosf(b.angle);
					float endY = b.y + b.length * sinf(b.angle);

					// Color: brown trunk -> green leaves
					float depthRatio
						= (float)b.depth / (float)maxDepth;
					uint8 r = (uint8)(100 * (1.0f - depthRatio)
						+ 30 * depthRatio);
					uint8 g = (uint8)(60 * (1.0f - depthRatio)
						+ 200 * depthRatio);
					uint8 bl = (uint8)(20 * (1.0f - depthRatio)
						+ 50 * depthRatio);
					SetHighColor(r, g, bl);

					// Pen size decreases with depth
					float penSize = (float)(maxDepth - b.depth)
						* 0.8f + 0.5f;
					SetPenSize(penSize);

					StrokeLine(BPoint(b.x, b.y),
						BPoint(endX, endY));

					// Branch into 2-3 sub-branches
					float shrink = 0.65f
						+ (float)(rand() % 15) / 100.0f;
					float spread = 0.35f
						+ (float)(rand() % 30) / 100.0f;

					if (stackSize < 4094) {
						stack[stackSize++] = {endX, endY,
							b.angle - spread,
							b.length * shrink, b.depth + 1};
						stack[stackSize++] = {endX, endY,
							b.angle + spread,
							b.length * shrink, b.depth + 1};

						// Occasional third branch
						if (rand() % 3 == 0 && stackSize < 4095) {
							float jitter = (float)(rand() % 20 - 10)
								* M_PI / 180.0f;
							stack[stackSize++] = {endX, endY,
								b.angle + jitter,
								b.length * shrink * 0.8f,
								b.depth + 1};
						}
					}
				}

				// Add leaf blossoms at tips
				SetDrawingMode(B_OP_ALPHA);
				for (int32 i = 0; i < 200; i++) {
					float lx = rand() % kBenchW;
					float ly = rand() % (kBenchH / 2)
						+ (kBenchH / 6);

					int32 flowerType = rand() % 3;
					uint8 fr, fg, fb;
					if (flowerType == 0) {
						fr = 255; fg = 180 + rand() % 75;
						fb = 200 + rand() % 55;
					} else if (flowerType == 1) {
						fr = 255; fg = 255;
						fb = 150 + rand() % 105;
					} else {
						fr = 200 + rand() % 55;
						fg = 100 + rand() % 80;
						fb = 200 + rand() % 55;
					}

					SetHighColor(fr, fg, fb, 80 + rand() % 120);
					float r = (rand() % 5) + 2;
					FillEllipse(BPoint(lx, ly), r, r);
				}
				SetDrawingMode(B_OP_COPY);
				SetPenSize(1.0f);
			}
			break;
		}

		case 18: // Plasma render — parallel pixel computation
		{
			uint32 cpuCount = _GetCpuCount();
			// Higher resolution for more work
			int32 pw = kBenchW;
			int32 ph = kBenchH;

			for (int32 t = 0; t < numOps; t++) {
				float timeOffset = (float)t * 0.7f
					+ (float)(rand() % 100) / 10.0f;

				BBitmap* plasma = new BBitmap(
					BRect(0, 0, pw - 1, ph - 1), B_RGBA32);

				if (plasma->InitCheck() == B_OK) {
					uint8* bits = (uint8*)plasma->Bits();
					int32 bpr = plasma->BytesPerRow();

					// Spawn one thread per CPU core
					int32 threadsToUse = (int32)cpuCount;
					thread_id* threads = new thread_id[threadsToUse];
					PlasmaThreadData* tdata
						= new PlasmaThreadData[threadsToUse];

					int32 rowsPerThread = ph / threadsToUse;

					for (int32 i = 0; i < threadsToUse; i++) {
						tdata[i].bits = bits;
						tdata[i].bpr = bpr;
						tdata[i].startY = i * rowsPerThread;
						tdata[i].endY = (i == threadsToUse - 1)
							? ph : (i + 1) * rowsPerThread;
						tdata[i].width = pw;
						tdata[i].height = ph;
						tdata[i].timeOffset = timeOffset;

						threads[i] = spawn_thread(_PlasmaWorker,
							"plasma_worker", B_NORMAL_PRIORITY,
							&tdata[i]);
						if (threads[i] >= 0)
							resume_thread(threads[i]);
					}

					// Wait for all threads
					for (int32 i = 0; i < threadsToUse; i++) {
						status_t result;
						wait_for_thread(threads[i], &result);
					}

					delete[] threads;
					delete[] tdata;

					// Draw to screen (window thread)
					DrawBitmap(plasma);
					delete plasma;
				}
			}
			break;
		}

		case 19: // EVERYTHING AT ONCE
		{
			for (int32 t = 0; t < numOps; t++) {
				// Black canvas
				SetHighColor(8, 8, 20);
				FillRect(Bounds());

				// Layer 1: Gradient background (tiled)
				for (int32 i = 0; i < 12; i++) {
					float x = rand() % kBenchW;
					float y = rand() % kBenchH;
					float w = (rand() % 250) + 100;
					float h = (rand() % 250) + 100;

					BGradientRadial grad(
						BPoint(x + w / 2, y + h / 2), w / 2);
					grad.AddColor((rgb_color){
						(uint8)(rand() % 100 + 50),
						(uint8)(rand() % 50),
						(uint8)(rand() % 150 + 100), 200},
						0.0f);
					grad.AddColor((rgb_color){0, 0, 0, 0}, 255.0f);

					SetDrawingMode(B_OP_ALPHA);
					FillEllipse(BRect(x, y, x + w, y + h), grad);
				}

				// Layer 2: Stars (filled)
				for (int32 i = 0; i < 30; i++) {
					SetHighColor(rand() % 256, rand() % 256,
						rand() % 256);
					float cx = rand() % kBenchW;
					float cy = rand() % kBenchH;
					float outerR = (rand() % 30) + 10;
					float innerR = outerR * 0.38f;

					BShape star;
					for (int32 p = 0; p < 20; p++) {
						float angle = (float)p * M_PI / 10.0f
							- M_PI / 2.0f;
						float r = (p % 2 == 0) ? outerR : innerR;
						float px = cx + r * cosf(angle);
						float py = cy + r * sinf(angle);
						if (p == 0)
							star.MoveTo(BPoint(px, py));
						else
							star.LineTo(BPoint(px, py));
					}
					star.Close();
					FillShape(&star);
				}

				// Layer 3: Bezier ribbons (alpha)
				for (int32 i = 0; i < 20; i++) {
					SetHighColor(rand() % 256, rand() % 256,
						rand() % 256, 80 + rand() % 100);
					SetPenSize((float)(rand() % 6) + 2.0f);

					BPoint cp[4];
					for (int32 j = 0; j < 4; j++)
						cp[j].Set(rand() % kBenchW,
							rand() % kBenchH);
					StrokeBezier(cp);
				}

				// Layer 4: Conic gradient circles
				for (int32 i = 0; i < 5; i++) {
					float cx = rand() % kBenchW;
					float cy = rand() % kBenchH;
					float r = (rand() % 80) + 40;

					BGradientConic grad(BPoint(cx, cy), 0.0f);
					grad.AddColor((rgb_color){255, 0, 0, 180},
						0.0f);
					grad.AddColor((rgb_color){0, 255, 0, 180},
						85.0f);
					grad.AddColor((rgb_color){0, 0, 255, 180},
						170.0f);
					grad.AddColor((rgb_color){255, 0, 0, 180},
						255.0f);
					FillEllipse(BPoint(cx, cy), r, r, grad);
				}

				// Layer 5: Text cloud (big, alpha, rotated)
				const char* words[] = {
					"HAIKU", "WATT", "GREEN", "FAST",
					"SAVE", "EARTH", "POWER", "ECO",
				};
				for (int32 i = 0; i < 40; i++) {
					SetHighColor(rand() % 256, rand() % 256,
						rand() % 256, 40 + rand() % 120);
					BFont font;
					font.SetSize((float)(rand() % 60) + 12.0f);
					font.SetFace(
						rand() % 2 ? B_BOLD_FACE : B_ITALIC_FACE);
					font.SetShear((float)(rand() % 50) + 65.0f);
					SetFont(&font);
					DrawString(words[rand() % 8],
						BPoint(rand() % kBenchW,
							rand() % kBenchH));
				}

				// Layer 6: Polygon mesh overlay
				for (int32 i = 0; i < 15; i++) {
					SetHighColor(rand() % 256, rand() % 256,
						rand() % 256, 60 + rand() % 80);
					float cx = rand() % kBenchW;
					float cy = rand() % kBenchH;
					float radius = (rand() % 60) + 20;
					int32 sides = (rand() % 8) + 5;

					BPoint pts[13];
					for (int32 s = 0; s < sides && s < 13; s++) {
						float angle = (float)s * 2.0f * M_PI
							/ (float)sides;
						float jitter = (float)(rand() % 20 - 10);
						pts[s].x
							= cx + (radius + jitter) * cosf(angle);
						pts[s].y
							= cy + (radius + jitter) * sinf(angle);
					}
					BPolygon poly(pts, sides > 13 ? 13 : sides);
					FillPolygon(&poly);
				}

				// Layer 7: Scaled bitmaps (plasma tiles)
				int32 tileSize = 32;
				BBitmap* tile = new BBitmap(
					BRect(0, 0, tileSize - 1, tileSize - 1),
					B_RGBA32);
				if (tile->InitCheck() == B_OK) {
					uint8* bits = (uint8*)tile->Bits();
					int32 bpr = tile->BytesPerRow();
					for (int32 y = 0; y < tileSize; y++) {
						for (int32 x = 0; x < tileSize; x++) {
							int32 off = y * bpr + x * 4;
							float v = sinf(x * 0.5f) + cosf(y * 0.5f
								+ t);
							bits[off + 0]
								= (uint8)((v * 0.5f + 0.5f) * 255);
							bits[off + 1] = (uint8)(x * 8);
							bits[off + 2] = (uint8)(y * 8);
							bits[off + 3] = 180;
						}
					}

					for (int32 i = 0; i < 20; i++) {
						float dx = rand() % kBenchW;
						float dy = rand() % kBenchH;
						float scale
							= (float)(rand() % 200 + 50) / 100.0f;
						DrawBitmap(tile,
							BRect(0, 0, tileSize - 1, tileSize - 1),
							BRect(dx, dy, dx + tileSize * scale,
								dy + tileSize * scale));
					}
					delete tile;
				}

				// Layer 8: Final highlight lines
				for (int32 i = 0; i < 50; i++) {
					SetHighColor(255, 255, 255, 20 + rand() % 40);
					SetPenSize((float)(rand() % 3) + 0.5f);
					StrokeLine(
						BPoint(rand() % kBenchW, rand() % kBenchH),
						BPoint(rand() % kBenchW, rand() % kBenchH));
				}

				SetDrawingMode(B_OP_COPY);
				SetPenSize(1.0f);
			}
			break;
		}

		// ==== LEVEL 6: SOFTWARE 3D TEAPOT ====

		case 20: // Wireframe teapot
		{
			// Generate teapot geometry once
			Tri3D* modelTris = new Tri3D[8192];
			int32 triCount = 0;
			_GenerateTeapot(modelTris, &triCount, 16);

			for (int32 t = 0; t < numOps; t++) {
				SetHighColor(5, 5, 15);
				FillRect(Bounds());

				float angle = (float)t * 6.0f * M_PI / 180.0f;
				SetPenSize(1.0f);
				_RenderTeapotSW(this, modelTris, triCount,
					angle, 0.3f, 600.0f,
					kBenchW / 2, kBenchH / 2, 8.0f,
					true, false);
				Sync();
			}

			delete[] modelTris;
			break;
		}

		case 21: // Flat-shaded teapot
		{
			Tri3D* modelTris = new Tri3D[8192];
			int32 triCount = 0;
			_GenerateTeapot(modelTris, &triCount, 20);

			for (int32 t = 0; t < numOps; t++) {
				SetHighColor(5, 5, 15);
				FillRect(Bounds());

				float angle = (float)t * 6.0f * M_PI / 180.0f;
				_RenderTeapotSW(this, modelTris, triCount,
					angle, 0.3f, 600.0f,
					kBenchW / 2, kBenchH / 2, 8.0f,
					false, true);
				Sync();
			}

			delete[] modelTris;
			break;
		}

		case 22: // 4 teapots rotating
		{
			Tri3D* modelTris = new Tri3D[8192];
			int32 triCount = 0;
			_GenerateTeapot(modelTris, &triCount, 16);

			for (int32 t = 0; t < numOps; t++) {
				SetHighColor(5, 5, 15);
				FillRect(Bounds());

				for (int32 tp = 0; tp < 4; tp++) {
					float cx = (tp % 2 == 0) ? kBenchW * 0.28f
						: kBenchW * 0.72f;
					float cy = (tp < 2) ? kBenchH * 0.32f
						: kBenchH * 0.72f;
					float angle = (float)t * 8.0f * M_PI / 180.0f
						+ tp * M_PI / 2.0f;
					float tilt = 0.3f + tp * 0.15f;

					_RenderTeapotSW(this, modelTris, triCount,
						angle, tilt, 350.0f, cx, cy, 8.0f,
						false, true);
				}
				Sync();
			}

			delete[] modelTris;
			break;
		}

		case 23: // Teapot army: 16 teapots in a grid
		{
			Tri3D* modelTris = new Tri3D[8192];
			int32 triCount = 0;
			_GenerateTeapot(modelTris, &triCount, 12);

			for (int32 t = 0; t < numOps; t++) {
				// Dark background with subtle gradient
				SetHighColor(5, 5, 20);
				FillRect(Bounds());

				// Floor grid (perspective lines)
				SetDrawingMode(B_OP_ALPHA);
				SetHighColor(30, 40, 60, 100);
				SetPenSize(1.0f);
				for (int32 l = 0; l < 20; l++) {
					float lx = (float)l * kBenchW / 20.0f;
					StrokeLine(BPoint(lx, kBenchH * 0.8f),
						BPoint(kBenchW / 2 + (lx - kBenchW / 2)
							* 0.3f, kBenchH * 0.15f));
				}
				for (int32 l = 0; l < 8; l++) {
					float ly = kBenchH * 0.8f
						- (float)l * kBenchH * 0.08f;
					float squeeze = 1.0f - (float)l * 0.08f;
					StrokeLine(
						BPoint(kBenchW * (0.5f - 0.5f * squeeze), ly),
						BPoint(kBenchW * (0.5f + 0.5f * squeeze), ly));
				}
				SetDrawingMode(B_OP_COPY);

				// Draw 16 teapots in a 4x4 grid, back to front
				for (int32 row = 0; row < 4; row++) {
					for (int32 col = 0; col < 4; col++) {
						int32 idx = row * 4 + col;

						// Perspective positioning
						float depth = (float)(3 - row);
						float perspScale
							= 1.0f / (1.0f + depth * 0.4f);
						float spacing = 180.0f * perspScale;

						float cx = kBenchW / 2
							+ (col - 1.5f) * spacing;
						float cy = kBenchH * 0.75f
							- row * kBenchH * 0.16f;
						float angle
							= (float)t * 5.0f * M_PI / 180.0f
							+ idx * M_PI / 8.0f;

						_RenderTeapotSW(this, modelTris, triCount,
							angle, 0.25f,
							200.0f * perspScale, cx, cy,
							8.0f, false, true);
					}
				}

				// Title overlay
				SetDrawingMode(B_OP_ALPHA);
				SetHighColor(255, 215, 0, 180);
				BFont titleFont(be_bold_font);
				titleFont.SetSize(14.0f);
				SetFont(&titleFont);
				DrawString("Software 3D \xE2\x80\x94 "
					"No GPU required. Pure Haiku.",
					BPoint(kBenchW / 2 - 170, kBenchH - 20));
				SetDrawingMode(B_OP_COPY);

				Sync();
			}

			delete[] modelTris;
			break;
		}
	}

	Sync();
	elapsed = system_time() - start;

	if (elapsed <= 0)
		elapsed = 1;

	fResults.opsPerSec[testIndex]
		= (float)numOps * 1000000.0f / (float)elapsed;

	// MB/s for fill-type operations
	if (testIndex == 0 || testIndex == 2 || testIndex == 11) {
		float avgArea = 100.0f * 100.0f;
		if (testIndex == 11)
			avgArea = 64.0f * 64.0f * 1.75f;
		fResults.mbPerSec[testIndex]
			= (float)numOps * avgArea * 4.0f / (float)elapsed;
	} else if (testIndex == 18) {
		// Plasma: full frame rendered
		fResults.mbPerSec[testIndex]
			= (float)numOps * kBenchW * kBenchH * 4.0f
			/ (float)elapsed;
	}
}


// #pragma mark - Bench2DPanel


static BStringView*
_MakeResultLabel(const char* name, const char* text, const rgb_color& color,
	float fontSize = 12.0f, bool bold = false)
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


Bench2DPanel::Bench2DPanel(BMessenger target)
	:
	BView("bench2dPanel", B_WILL_DRAW | B_SUPPORTS_LAYOUT),
	fBenchView(NULL),
	fBenchWin(NULL),
	fBenchThread(-1),
	fTarget(target),
	fDriverLabel(NULL),
	fDeviceLabel(NULL),
	fAccelStatusLabel(NULL),
	fHooksLabel(NULL),
	fStatusLabel(NULL),
	fTempLabel(NULL),
	fTempRunner(NULL)
{
	memset(fLevelLabels, 0, sizeof(fLevelLabels));
	memset(fTestLabels, 0, sizeof(fTestLabels));

	SetViewColor(kDarkBg);
	BView* topView = this;

	// System info panel
	BBox* sysBox = new BBox("b2dSysBox");
	sysBox->SetViewColor(kDarkBg);
	sysBox->SetLowColor(kDarkBg);

	BStringView* sysTitle = _MakeResultLabel("b2dSysTitle",
		B_TRANSLATE("2D Driver"), kLabelWhite2D, 12.0f, true);
	sysTitle->SetViewColor(kDarkBg);
	sysTitle->SetLowColor(kDarkBg);
	sysBox->SetLabel(sysTitle);

	BView* sysInner = new BView("b2dSysInner", B_WILL_DRAW);
	sysInner->SetViewColor(kDarkBg);

	fDriverLabel = _MakeResultLabel("driver",
		B_TRANSLATE("Driver: detecting..."), kWhite2D, 11.0f, true);
	fDeviceLabel = _MakeResultLabel("device", B_TRANSLATE("Device: --"),
		kWhite2D, 11.0f);
	fAccelStatusLabel = _MakeResultLabel("accel",
		B_TRANSLATE("2D Acceleration: detecting..."),
		kGray2D, 11.0f, true);
	fHooksLabel = _MakeResultLabel("hooks", "",
		kGray2D, 10.0f);
	fTempLabel = _MakeResultLabel("temp", B_TRANSLATE("Temp: reading..."),
		kGreen2D, 10.0f, true);

	BLayoutBuilder::Group<>(sysInner, B_VERTICAL, 2)
		.SetInsets(8, 8, 8, 8)
		.Add(fDriverLabel)
		.Add(fDeviceLabel)
		.Add(fAccelStatusLabel)
		.Add(fHooksLabel)
		.Add(fTempLabel)
	.End();
	sysBox->AddChild(sysInner);

	// Results panel
	BBox* resBox = new BBox("b2dResBox");
	resBox->SetViewColor(kDarkBg);
	resBox->SetLowColor(kDarkBg);

	BStringView* resTitle = _MakeResultLabel("b2dResTitle",
		B_TRANSLATE("Benchmark Results"), kLabelWhite2D, 12.0f, true);
	resTitle->SetViewColor(kDarkBg);
	resTitle->SetLowColor(kDarkBg);
	resBox->SetLabel(resTitle);

	BView* resInner = new BView("b2dResInner", B_WILL_DRAW);
	resInner->SetViewColor(kDarkBg);

	for (int32 l = 0; l < kNumBench2DLevels; l++) {
		BString name;
		name.SetToFormat("lvl%d", (int)l);
		fLevelLabels[l] = _MakeResultLabel(name.String(),
			kLevelNames[l], kLevelColors[l], 11.0f, true);
	}

	BFont monoFont(be_fixed_font);
	monoFont.SetSize(11.0f);

	for (int32 i = 0; i < kNumBench2DTests; i++) {
		BString name;
		name.SetToFormat("test%d", (int)i);
		BString text;
		text.SetToFormat("  %-22s  --", kTestNames[i]);
		fTestLabels[i] = _MakeResultLabel(name.String(), text.String(),
			kGray2D, 11.0f);
		fTestLabels[i]->SetFont(&monoFont);
	}

	BLayoutBuilder::Group<>(resInner, B_HORIZONTAL, 10)
		.SetInsets(8, 8, 8, 8)
		// Left column: Levels 1-3
		.AddGroup(B_VERTICAL, 1)
			.Add(fLevelLabels[0])
			.Add(fTestLabels[0])
			.Add(fTestLabels[1])
			.Add(fTestLabels[2])
			.Add(fTestLabels[3])
			.Add(new BSeparatorView(B_HORIZONTAL))
			.Add(fLevelLabels[1])
			.Add(fTestLabels[4])
			.Add(fTestLabels[5])
			.Add(fTestLabels[6])
			.Add(fTestLabels[7])
			.Add(new BSeparatorView(B_HORIZONTAL))
			.Add(fLevelLabels[2])
			.Add(fTestLabels[8])
			.Add(fTestLabels[9])
			.Add(fTestLabels[10])
			.Add(fTestLabels[11])
			.AddGlue()
		.End()
		.Add(new BSeparatorView(B_VERTICAL))
		// Right column: Levels 4-6
		.AddGroup(B_VERTICAL, 1)
			.Add(fLevelLabels[3])
			.Add(fTestLabels[12])
			.Add(fTestLabels[13])
			.Add(fTestLabels[14])
			.Add(fTestLabels[15])
			.Add(new BSeparatorView(B_HORIZONTAL))
			.Add(fLevelLabels[4])
			.Add(fTestLabels[16])
			.Add(fTestLabels[17])
			.Add(fTestLabels[18])
			.Add(fTestLabels[19])
			.Add(new BSeparatorView(B_HORIZONTAL))
			.Add(fLevelLabels[5])
			.Add(fTestLabels[20])
			.Add(fTestLabels[21])
			.Add(fTestLabels[22])
			.Add(fTestLabels[23])
			.AddGlue()
		.End()
	.End();
	resBox->AddChild(resInner);

	// Status + button
	fStatusLabel = _MakeResultLabel("status",
		B_TRANSLATE("24 tests, 6 levels. From rectangles to software 3D "
			"teapots."),
		kGray2D, 10.0f);

	BButton* runButton = new BButton("runBtn",
		B_TRANSLATE("Run 2D Benchmark"),
		new BMessage(kMsgBench2DStart));

	BLayoutBuilder::Group<>(topView, B_VERTICAL, 6)
		.SetInsets(12, 12, 12, 12)
		.Add(sysBox)
		.Add(resBox)
		.AddGlue()
		.Add(new BSeparatorView(B_HORIZONTAL))
		.AddGroup(B_HORIZONTAL, 5)
			.Add(fStatusLabel)
			.AddGlue()
			.Add(runButton)
		.End()
	.End();
}


Bench2DPanel::~Bench2DPanel()
{
	delete fTempRunner;
	if (fBenchThread >= 0) {
		status_t result;
		wait_for_thread(fBenchThread, &result);
	}
}


void
Bench2DPanel::AttachedToWindow()
{
	BView::AttachedToWindow();

	// The Run button's message must reach this panel, not the window looper.
	if (BButton* button = dynamic_cast<BButton*>(FindView("runBtn")))
		button->SetTarget(this);

	// Start periodic temperature updates (BMessenger(this) is valid now
	// that the view is attached to a looper).
	if (fTempRunner == NULL) {
		BMessage tempMsg(kMsgBench2DTempUpd);
		fTempRunner = new BMessageRunner(BMessenger(this), &tempMsg, 2000000);
	}

	// The benchmark is NOT auto-started: as a tab panel this view is attached
	// as soon as the deck is built, long before the user opens the tab. The
	// run is triggered by the panel's Run button or by the deck (RunTab).
}


void
Bench2DPanel::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgBench2DStart:
		{
			if (fBenchThread >= 0)
				break;

			// Open the drawing window for the benchmark
			fBenchWin = new BWindow(
				BRect(50, 50, 50 + kBenchW - 1, 50 + kBenchH - 1),
				"HaikuBench \xE2\x80\x94 2D Benchmark Running...",
				B_TITLED_WINDOW,
				B_NOT_CLOSABLE | B_NOT_ZOOMABLE | B_NOT_RESIZABLE);

			fBenchView = new Bench2DView();
			fBenchWin->AddChild(fBenchView);
			fBenchWin->Show();

			// Run benchmark in a separate thread so the
			// message loop stays responsive.
			fBenchThread = spawn_thread(_BenchThread,
				"2d_benchmark", B_NORMAL_PRIORITY, this);
			if (fBenchThread >= 0)
				resume_thread(fBenchThread);
			break;
		}

		case kMsgBench2DDone:
		{
			// Wait for the benchmark thread to finish
			if (fBenchThread >= 0) {
				status_t result;
				wait_for_thread(fBenchThread, &result);
				fBenchThread = -1;
			}

			// Cache results before closing the drawing window —
			// fBenchView will be destroyed with fBenchWin, and
			// BStrings in AccelInfo would become dangling.
			if (fBenchWin != NULL && fBenchWin->Lock()) {
				fCachedResults = fBenchView->Results();
				fCachedAccelInfo = fBenchView->AccelInfo();
				fCachedDriverInfo = fBenchView->DriverInfo();
				fBenchWin->Unlock();
			}

			// Close drawing window
			if (fBenchWin != NULL) {
				fBenchWin->PostMessage(B_QUIT_REQUESTED);
				fBenchWin = NULL;
				fBenchView = NULL;
			}

			// Populate and show results window
			_UpdateResultLabels();
			fStatusLabel->SetText(B_TRANSLATE(
				"Done. From rectangles to software 3D teapots "
				"\xE2\x80\x94 who needs a GPU?"));

			// Report results to the main window. The former standalone window
			// did this from QuitRequested (on close); a tab panel never
			// closes, so we send on completion instead.
			if (fCachedResults.valid) {
				BMessage msg(kMsgBench2DResult);
				for (int32 i = 0; i < kNumBench2DTests; i++) {
					msg.AddFloat("ops_per_sec", fCachedResults.opsPerSec[i]);
					msg.AddFloat("mb_per_sec", fCachedResults.mbPerSec[i]);
				}
				fTarget.SendMessage(&msg);
			}
			break;
		}

		case kMsgBench2DTempUpd:
			_UpdateTemperature();
			break;

		default:
			BView::MessageReceived(message);
			break;
	}
}


void
Bench2DPanel::_UpdateResultLabels()
{
	// Use cached copies — fBenchView may have been destroyed
	// when the benchmark drawing window was closed.
	Bench2DResults r = fCachedResults;
	Accel2DInfo accel = fCachedAccelInfo;
	BString text;

	// Driver label — prefer accelerant name from ioctl (reliable),
	// fall back to listimage only if ioctl didn't work
	if (accel.accelerantName.Length() > 0) {
		// Check if it's a patched (non-packaged) driver
		BString driverText = fCachedDriverInfo;
		if (driverText.FindFirst("PATCHED") >= 0)
			text.SetToFormat("Driver: %s (PATCHED)",
				accel.accelerantName.String());
		else
			text.SetToFormat("Driver: %s",
				accel.accelerantName.String());
	} else {
		text.SetToFormat("Driver: %s", fCachedDriverInfo.String());
	}
	fDriverLabel->SetText(text.String());

	// Device info (name, chipset, VRAM)
	BString devText;
	if (accel.deviceName.Length() > 0 || accel.chipset.Length() > 0) {
		if (accel.deviceName.Length() > 0
			&& accel.chipset.Length() > 0) {
			devText.SetToFormat("Device: %s | %s",
				accel.deviceName.String(), accel.chipset.String());
		} else if (accel.deviceName.Length() > 0) {
			devText.SetToFormat("Device: %s",
				accel.deviceName.String());
		} else {
			devText.SetToFormat("Device: %s",
				accel.chipset.String());
		}
	} else if (accel.accelerantName.Length() > 0) {
		// No BScreen info, but we know the accelerant
		devText.SetToFormat("Device: %s (via accelerant)",
			accel.accelerantName.String());
	}

	if (accel.vramBytes > 0) {
		BString vram;
		if (accel.vramBytes >= 1024 * 1024)
			vram.SetToFormat(" | VRAM: %" B_PRIu32 " MB",
				accel.vramBytes / (1024 * 1024));
		else
			vram.SetToFormat(" | VRAM: %" B_PRIu32 " KB",
				accel.vramBytes / 1024);
		devText.Append(vram);
	}

	if (devText.Length() > 0) {
		fDeviceLabel->SetText(devText.String());
		fDeviceLabel->SetHighColor(kWhite2D);
	} else {
		fDeviceLabel->SetText(B_TRANSLATE("Device: not detected"));
		fDeviceLabel->SetHighColor(kGray2D);
	}

	// 2D Acceleration status
	if (accel.isHardwareAccel) {
		fAccelStatusLabel->SetText(
			B_TRANSLATE("2D Acceleration: ACTIVE (hardware)"));
		fAccelStatusLabel->SetHighColor(kGreen2D);
	} else {
		fAccelStatusLabel->SetText(
			B_TRANSLATE("2D Acceleration: NONE (software rendering)"));
		fAccelStatusLabel->SetHighColor(kRed2D);
	}

	// Hook details (only if clone detection succeeded)
	if (accel.hooksChecked) {
		text.SetToFormat("Hooks: FillRect %s | ScreenBlit %s "
			"| InvertRect %s",
			accel.hasFillRect ? "YES" : "no",
			accel.hasScreenBlit ? "YES" : "no",
			accel.hasInvertRect ? "YES" : "no");
		fHooksLabel->SetText(text.String());
		fHooksLabel->SetHighColor(
			accel.isHardwareAccel ? kCyan2D : kGray2D);
	} else {
		fHooksLabel->SetText(
			B_TRANSLATE("Hooks: detection via heuristic (clone failed)"));
		fHooksLabel->SetHighColor(kGray2D);
	}

	// Test results
	for (int32 i = 0; i < kNumBench2DTests; i++) {
		int32 level = i / 4;
		float ops = r.opsPerSec[i];
		const char* unit = (level == 5) ? "fps" : "ops/s";

		if (ops >= 10000.0f)
			text.SetToFormat("  %-22s  %8.0f %s",
				kTestNames[i], ops, unit);
		else
			text.SetToFormat("  %-22s  %8.1f %s",
				kTestNames[i], ops, unit);

		fTestLabels[i]->SetHighColor(kWhite2D);
		fTestLabels[i]->SetText(text.String());
	}
}


void
Bench2DPanel::_UpdateTemperature()
{
	const char* zoneNames[] = {"CPU", "GPU"};
	BString tempText;
	float maxTemp = 0.0f;

	for (int32 i = 0; i < 8; i++) {
		char path[128];
		snprintf(path, sizeof(path), "/dev/power/acpi_thermal/%" B_PRId32,
			i);

		FILE* file = fopen(path, "r");
		if (file == NULL)
			break;

		char buffer[256];
		size_t bytesRead = fread(buffer, 1, sizeof(buffer) - 1, file);
		fclose(file);

		if (bytesRead == 0)
			break;

		buffer[bytesRead] = '\0';

		float current = -1.0f;
		const char* curPtr = strstr(buffer, "Current Temperature:");
		if (curPtr != NULL)
			sscanf(curPtr, "Current Temperature: %f", &current);

		if (current >= 0.0f) {
			if (tempText.Length() > 0)
				tempText.Append(" | ");

			const char* label = (i < 2) ? zoneNames[i] : "Zone";
			BString zone;
			if (i >= 2)
				zone.SetToFormat("%s%" B_PRId32 ": %.0f\xC2\xB0""C",
					label, i, current);
			else
				zone.SetToFormat("%s: %.0f\xC2\xB0""C", label, current);
			tempText.Append(zone);

			if (current > maxTemp)
				maxTemp = current;
		}
	}

	if (tempText.Length() > 0) {
		rgb_color tempColor = kGreen2D;
		if (maxTemp >= 80.0f)
			tempColor = (rgb_color){255, 80, 80, 255};
		else if (maxTemp >= 65.0f)
			tempColor = (rgb_color){230, 200, 50, 255};

		fTempLabel->SetHighColor(tempColor);
		fTempLabel->SetText(tempText.String());
	} else {
		fTempLabel->SetHighColor(kGray2D);
		fTempLabel->SetText(B_TRANSLATE("Temp: N/A"));
	}
}


/*static*/ int32
Bench2DPanel::_BenchThread(void* data)
{
	Bench2DPanel* window = static_cast<Bench2DPanel*>(data);

	// Give the benchmark window time to show
	snooze(200000);

	if (window->fBenchWin != NULL && window->fBenchWin->Lock()) {
		window->fBenchView->RunBenchmark();
		window->fBenchWin->Unlock();
	}

	// Notify the results window that we're done
	BMessenger(window).SendMessage(kMsgBench2DDone);
	return 0;
}
