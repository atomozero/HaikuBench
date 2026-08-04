/*
 * Copyright 2025 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 */


#include "BenchDeckWindow.h"

#include <Catalog.h>
#include <LayoutBuilder.h>
#include <Message.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "BenchDeck"

#include "Bench2DWindow.h"
#include "GpuBenchWindow.h"
#include "TeapotWindow.h"
#include "VulkanBenchWindow.h"


// Posted to our own looper by _RequestTab so the tab switch and benchmark
// start run on the deck's thread, not the caller's.
static const uint32 kMsgDeckSelectTab = 'dstb';

static const rgb_color kDarkBg = {13, 17, 23, 255};

static const char* kTabTitles[] = {
	"2D", "GPU", "Vulkan", "Teapot 3D"
};

static const char* kTabSubtitles[] = {
	B_TRANSLATE("2D drawing benchmark (app_server)"),
	B_TRANSLATE("OpenGL GPU benchmark"),
	B_TRANSLATE("Vulkan compute benchmark"),
	B_TRANSLATE("Interactive OpenGL teapots")
};


//	#pragma mark - DeckTabView


DeckTabView::DeckTabView(const char* name, BenchDeckWindow* deck)
	:
	BTabView(name, B_WIDTH_FROM_LABEL),
	fDeck(deck)
{
	SetViewColor(kDarkBg);
	SetLowColor(kDarkBg);
}


void
DeckTabView::Select(int32 index)
{
	BTabView::Select(index);
	if (fDeck != NULL)
		fDeck->TabActivated(index);
}


//	#pragma mark - BenchDeckWindow


BenchDeckWindow::BenchDeckWindow(BMessenger target)
	:
	BWindow(BRect(90, 70, 780, 620),
		"HaikuBench \xE2\x80\x94 Benchmarks",
		B_TITLED_WINDOW,
		B_AUTO_UPDATE_SIZE_LIMITS),
	fActiveTab(-1)
{
	fHeader = new HeaderView("deckHeader");
	fHeader->SetState(kHeaderIdle);
	fHeader->SetSubtitle(B_TRANSLATE("Benchmarks"));

	fTabView = new DeckTabView("deckTabs", this);

	f2DPanel = new Bench2DPanel(target);
	fGpuPanel = new GpuBenchPanel(target);
	fVkPanel = new VulkanBenchPanel(target);
	fTeapotPanel = new TeapotPanel(target);

	fTabView->AddTab(f2DPanel);
	fTabView->AddTab(fGpuPanel);
	fTabView->AddTab(fVkPanel);
	fTabView->AddTab(fTeapotPanel);

	fTabView->TabAt(kDeckTab2D)->SetLabel(kTabTitles[kDeckTab2D]);
	fTabView->TabAt(kDeckTabGpu)->SetLabel(kTabTitles[kDeckTabGpu]);
	fTabView->TabAt(kDeckTabVulkan)->SetLabel(kTabTitles[kDeckTabVulkan]);
	fTabView->TabAt(kDeckTabTeapot)->SetLabel(kTabTitles[kDeckTabTeapot]);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(fHeader)
		.Add(fTabView)
	.End();
}


BenchDeckWindow::~BenchDeckWindow()
{
}


bool
BenchDeckWindow::QuitRequested()
{
	// Never let a running benchmark be torn out from under its thread.
	if (f2DPanel->IsBusy() || fGpuPanel->IsBusy()
		|| fVkPanel->IsBusy() || fTeapotPanel->IsBusy()) {
		return false;
	}

	// Persist the window: hide instead of quitting so GL contexts and detected
	// device info survive for the next open. Stop the active tab's animation
	// first so a hidden deck costs no CPU.
	if (fActiveTab == kDeckTabTeapot)
		fTeapotPanel->StopRendering();
	else if (fActiveTab == kDeckTabGpu)
		fGpuPanel->StopPreview();

	if (!IsHidden())
		Hide();
	fActiveTab = -1;
	return false;
}


void
BenchDeckWindow::ShowTab(int32 index)
{
	_RequestTab(index, false);
}


void
BenchDeckWindow::RunTab(int32 index)
{
	_RequestTab(index, true);
}


void
BenchDeckWindow::_RequestTab(int32 index, bool run)
{
	// Show()/Activate() are the standard "reveal a window from another thread"
	// idiom and start the looper on first use. The actual tab switch and
	// benchmark start are deferred to our own thread via a posted message, so
	// no view is touched across threads.
	if (IsHidden())
		Show();
	Activate(true);

	BMessage msg(kMsgDeckSelectTab);
	msg.AddInt32("tab", index);
	msg.AddBool("run", run);
	PostMessage(&msg, this);
}


void
BenchDeckWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgDeckSelectTab:
		{
			int32 index = message->GetInt32("tab", kDeckTab2D);
			bool run = message->GetBool("run", false);

			// Runs on the deck's thread — safe to touch views. Select routes
			// through DeckTabView::Select -> TabActivated (GL pause/resume).
			fTabView->Select(index);

			if (run) {
				switch (index) {
					case kDeckTab2D:
						BMessenger(f2DPanel).SendMessage(kMsgBench2DStart);
						break;
					case kDeckTabGpu:
						BMessenger(fGpuPanel).SendMessage(kMsgGpuBenchStart);
						break;
					case kDeckTabVulkan:
						BMessenger(fVkPanel).SendMessage(kMsgVkBenchStart);
						break;
					default:
						break;
				}
			}
			break;
		}

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


void
BenchDeckWindow::TabActivated(int32 index)
{
	if (index == fActiveTab)
		return;

	// Pause the animation on the tab we are leaving.
	if (fActiveTab == kDeckTabTeapot)
		fTeapotPanel->StopRendering();
	else if (fActiveTab == kDeckTabGpu)
		fGpuPanel->StopPreview();

	fActiveTab = index;

	// Resume animation on the tab we are entering.
	if (index == kDeckTabTeapot)
		fTeapotPanel->StartRendering();
	else if (index == kDeckTabGpu)
		fGpuPanel->StartPreview();

	if (index >= 0 && index <= kDeckTabTeapot)
		fHeader->SetSubtitle(kTabSubtitles[index]);
}
