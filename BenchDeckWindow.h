/*
 * Copyright 2025 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef BENCH_DECK_WINDOW_H
#define BENCH_DECK_WINDOW_H


#include <Messenger.h>
#include <TabView.h>
#include <Window.h>

#include "HeaderView.h"

class Bench2DPanel;
class GpuBenchPanel;
class VulkanBenchPanel;
class TeapotPanel;
class BenchDeckWindow;


// Tab order in the deck. Kept in one place so the main window and the deck
// agree on which index maps to which benchmark.
enum {
	kDeckTab2D		= 0,
	kDeckTabGpu		= 1,
	kDeckTabVulkan	= 2,
	kDeckTabTeapot	= 3
};


// A BTabView that tells the deck when the selection changes, so the deck can
// pause the GL animation of the tab being left and resume it on the tab being
// entered. BTabView has no built-in change notification, so we override
// Select().
class DeckTabView : public BTabView {
public:
								DeckTabView(const char* name,
									BenchDeckWindow* deck);
	virtual	void				Select(int32 index);

private:
			BenchDeckWindow*	fDeck;
};


// The single window that hosts all four benchmarks as tabs, replacing the
// former four standalone windows. It carries the same slate HeaderView banner
// as the main window and forwards benchmark results to the main window via the
// target messenger (each panel already does this itself).
//
// The window is a persistent singleton owned by the main window: closing it
// hides it (so its GL contexts and detected device info survive) rather than
// quitting.
class BenchDeckWindow : public BWindow {
public:
								BenchDeckWindow(BMessenger target);
	virtual						~BenchDeckWindow();

	virtual	void				MessageReceived(BMessage* message);
	virtual	bool				QuitRequested();

			// Bring the deck to the front and switch to the given tab
			// (kDeckTab* constant). Creates nothing — the panels already
			// exist.
			void				ShowTab(int32 index);

			// Like ShowTab, but also kicks off that benchmark (posts the
			// panel's Start message). Teapot has no discrete run — it just
			// becomes visible and animates. Used by the main window's
			// per-benchmark buttons and the Run All sequence.
			void				RunTab(int32 index);

			// Called by DeckTabView when the visible tab changes. Pauses the
			// GL animation of the previous tab and resumes it on the new one.
			void				TabActivated(int32 index);

private:
			// Reveal the deck and post a tab-switch to our own thread (so the
			// actual view manipulation never happens on the caller's thread).
			void				_RequestTab(int32 index, bool run);

			HeaderView*			fHeader;
			DeckTabView*		fTabView;

			Bench2DPanel*		f2DPanel;
			GpuBenchPanel*		fGpuPanel;
			VulkanBenchPanel*	fVkPanel;
			TeapotPanel*		fTeapotPanel;

			int32				fActiveTab;
};


#endif // BENCH_DECK_WINDOW_H
