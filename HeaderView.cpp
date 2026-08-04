/*
 * Copyright 2025 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 */


#include "HeaderView.h"

#include <Bitmap.h>
#include <Catalog.h>
#include <Font.h>
#include <IconUtils.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Header"

#include "IconData.h"


// Visual constants. Same slate/title/subtitle tones LANterna uses, so the two
// apps look like they belong to the same family on the desktop.
static const rgb_color kHeaderBg		= {40, 50, 65, 255};
static const rgb_color kHeaderTitle		= {245, 245, 245, 255};
static const rgb_color kHeaderSubtitle	= {180, 195, 210, 255};
static const rgb_color kDotStroke		= {255, 255, 255, 255};

// Fallback fill for the logo tile if HVIF rasterization fails.
static const rgb_color kLogoFill		= {90, 155, 213, 255};

// Status accents for the dot overlaid on the tile.
static const rgb_color kAccentIdle		= {160, 160, 160, 255};
static const rgb_color kAccentProgress	= {224, 160, 48, 255};
static const rgb_color kAccentDone		= {90, 200, 120, 255};
static const rgb_color kAccentError		= {220, 80, 80, 255};

static const float kHeaderHeight		= 64.0f;
static const float kIconX				= 14.0f;
static const float kIconY				= 12.0f;
static const float kIconSize			= 40.0f;
static const float kTextX				= 68.0f;
static const float kTitleBaselineY		= 27.0f;
static const float kSubtitleBaselineY	= 47.0f;


static rgb_color
_AccentFor(HeaderState state)
{
	switch (state) {
		case kHeaderProgress:
			return kAccentProgress;
		case kHeaderDone:
			return kAccentDone;
		case kHeaderError:
			return kAccentError;
		case kHeaderIdle:
		default:
			return kAccentIdle;
	}
}


HeaderView::HeaderView(const char* name)
	:
	BView(name, B_WILL_DRAW | B_SUPPORTS_LAYOUT | B_FULL_UPDATE_ON_RESIZE),
	fState(kHeaderIdle),
	fSubtitle(""),
	fCachedIcon(NULL),
	fCachedIconSize(0.0f)
{
	SetViewColor(kHeaderBg);
	SetLowColor(kHeaderBg);
}


HeaderView::~HeaderView()
{
	delete fCachedIcon;
}


void
HeaderView::SetState(HeaderState state)
{
	if (state == fState)
		return;
	fState = state;
	Invalidate();
}


void
HeaderView::SetSubtitle(const char* text)
{
	BString next(text != NULL ? text : "");
	if (next == fSubtitle)
		return;
	fSubtitle = next;
	Invalidate();
}


void
HeaderView::Draw(BRect /*updateRect*/)
{
	BRect bounds = Bounds();

	SetHighColor(kHeaderBg);
	FillRect(bounds);

	BRect iconRect(kIconX, kIconY, kIconX + kIconSize - 1,
		kIconY + kIconSize - 1);
	_DrawLogoTile(iconRect);
	_DrawStatusDot(iconRect);

	// B_OP_OVER so the antialiased glyphs blend cleanly against the slate.
	SetDrawingMode(B_OP_OVER);

	BFont titleFont(be_bold_font);
	titleFont.SetSize(18.0f);
	SetFont(&titleFont);
	SetHighColor(kHeaderTitle);
	DrawString("HaikuBench", BPoint(kTextX, kTitleBaselineY));

	BFont subFont(be_plain_font);
	subFont.SetSize(11.0f);
	SetFont(&subFont);
	SetHighColor(kHeaderSubtitle);
	DrawString(fSubtitle.String(), BPoint(kTextX, kSubtitleBaselineY));
}


// Rasterize the brand HVIF into a square RGBA BBitmap of `size` pixels.
// Returns NULL if rasterization fails. Caller owns the result.
static BBitmap*
_RenderHvif(float size)
{
	BBitmap* bitmap = new BBitmap(BRect(0, 0, size - 1, size - 1), 0,
		B_RGBA32);
	if (bitmap == NULL || bitmap->InitCheck() != B_OK) {
		delete bitmap;
		return NULL;
	}
	status_t result = BIconUtils::GetVectorIcon(
		(const uint8*)kIconHvif, kIconHvifSize, bitmap);
	if (result != B_OK) {
		delete bitmap;
		return NULL;
	}
	return bitmap;
}


void
HeaderView::_DrawLogoTile(BRect rect)
{
	// Rasterizing the HVIF is expensive (BIconUtils + RGBA allocation). The
	// cache holds until the requested size changes -- Draw() runs on every
	// Invalidate, i.e. on every state/subtitle change and every resize.
	if (fCachedIcon == NULL || fCachedIconSize != rect.Width()) {
		delete fCachedIcon;
		fCachedIcon = _RenderHvif(rect.Width());
		fCachedIconSize = rect.Width();
	}

	if (fCachedIcon != NULL) {
		// Alpha-blend so the icon's transparent pixels let the slate show
		// through.
		SetDrawingMode(B_OP_ALPHA);
		SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
		DrawBitmap(fCachedIcon, rect.LeftTop());
		SetDrawingMode(B_OP_COPY);
		return;
	}

	// Fallback: rounded brand-colored tile if the HVIF could not rasterize.
	float radius = rect.Width() * 0.18f;
	SetHighColor(kLogoFill);
	FillRoundRect(rect, radius, radius);
}


void
HeaderView::_DrawStatusDot(BRect iconRect)
{
	// Filled circle overlaid on the tile's bottom-right corner, stroked white
	// so it stays crisp against any color.
	const float dotSize = 14.0f;
	BRect dot(0, 0, dotSize - 1, dotSize - 1);
	dot.OffsetTo(iconRect.right - dotSize + 4, iconRect.bottom - dotSize + 4);

	SetDrawingMode(B_OP_ALPHA);
	SetHighColor(_AccentFor(fState));
	FillEllipse(dot);
	SetHighColor(kDotStroke);
	StrokeEllipse(dot);
	SetDrawingMode(B_OP_COPY);
}


BSize
HeaderView::MinSize()
{
	return BSize(360.0f, kHeaderHeight);
}


BSize
HeaderView::MaxSize()
{
	return BSize(B_SIZE_UNLIMITED, kHeaderHeight);
}


BSize
HeaderView::PreferredSize()
{
	return BSize(540.0f, kHeaderHeight);
}


BBitmap*
HeaderView::MakeLogoBitmap(float size)
{
	return _RenderHvif(size);
}
