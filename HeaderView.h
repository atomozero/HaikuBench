/*
 * Copyright 2025 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef HEADER_VIEW_H
#define HEADER_VIEW_H


#include <InterfaceDefs.h>
#include <String.h>
#include <View.h>

class BBitmap;


// Visual state of the banner. Drives the color of the status dot overlaid on
// the logo tile, mapped to HaikuBench's run lifecycle:
//   kHeaderIdle      -> nothing running                     (grey)
//   kHeaderProgress  -> a benchmark is running              (amber)
//   kHeaderDone      -> last run finished successfully      (green)
//   kHeaderError     -> something failed                    (red)
enum HeaderState {
	kHeaderIdle,
	kHeaderProgress,
	kHeaderDone,
	kHeaderError
};


// The dark banner at the top of the main window. Same identity as LANterna:
// a slate background, a colored "logo tile" on the left with the run-state dot
// overlaid on it, and to the right the app name plus a subtitle line
// (e.g. "Ready.", "Running: CPU Integer (3/20)...", "System benchmark
// complete!"). Everything is drawn by hand (no child views) so the colors
// tell the state at a glance.
class HeaderView : public BView {
public:
								HeaderView(const char* name);
	virtual						~HeaderView();

			// Update the run state (drives the dot color) and the metadata
			// line. Both calls are idempotent.
			void				SetState(HeaderState state);
			void				SetSubtitle(const char* text);

	virtual	void				Draw(BRect updateRect);
	virtual	BSize				MinSize();
	virtual	BSize				MaxSize();
	virtual	BSize				PreferredSize();

	// Rasterize the same logo tile as the banner into an RGBA BBitmap of the
	// requested size. Caller owns the bitmap (may return NULL on error).
	static	BBitmap*			MakeLogoBitmap(float size);

private:
			void				_DrawLogoTile(BRect rect);
			void				_DrawStatusDot(BRect iconRect);

			HeaderState			fState;
			BString				fSubtitle;

	// Cache of the rasterized HVIF. Reallocated only when the requested size
	// changes: a redraw at the same size becomes a flat DrawBitmap instead of
	// another vector rasterization + RGBA allocation.
			BBitmap*			fCachedIcon;
			float				fCachedIconSize;
};


#endif // HEADER_VIEW_H
