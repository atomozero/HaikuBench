/*
 * Copyright 2025 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 */


#include "MainWindow.h"

#include <Alert.h>
#include <Box.h>
#include <DateTime.h>
#include <FindDirectory.h>
#include <GroupLayout.h>
#include <LayoutBuilder.h>
#include <Message.h>
#include <Messenger.h>
#include <Path.h>
#include <SeparatorView.h>
#include <StringView.h>

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "Bench2DWindow.h"
#include "GpuBenchWindow.h"
#include "ScoreBaseline.h"
#include "SysBenchmark.h"
#include "TeapotWindow.h"
#include "Version.h"
#include "VulkanBenchWindow.h"


static const rgb_color kDarkBg = {13, 17, 23, 255};
static const rgb_color kGreen = {57, 211, 83, 255};
static const rgb_color kWhite = {230, 237, 243, 255};
static const rgb_color kGray = {170, 180, 195, 255};
static const rgb_color kLabelWhite = {200, 210, 225, 255};
static const rgb_color kYellow = {230, 200, 50, 255};
static const rgb_color kCyan = {80, 200, 220, 255};
static const rgb_color kOrange = {230, 150, 50, 255};
static const rgb_color kPurple = {180, 130, 230, 255};


static BStringView*
_MakeLabel(const char* name, const char* text, const rgb_color& color,
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


MainWindow::MainWindow()
	:
	BWindow(BRect(80, 60, 860, 560),
		"HaikuBench " HAIKUBENCH_VERSION
			" \xE2\x80\x94 System Benchmark Suite",
		B_TITLED_WINDOW,
		B_AUTO_UPDATE_SIZE_LIMITS | B_QUIT_ON_WINDOW_CLOSE),
	fBench2DFPS(0.0f),
	fGpuScore(0.0f),
	fVkScore(0.0f),
	fTempRunner(NULL),
	fSysBenchThread(-1),
	fRunAllState(-1),
	fCurrentSysTest(-1)
{
	memset(&fSysResults, 0, sizeof(fSysResults));
	_BuildLayout();

	// Start temperature timer
	BMessage tempMsg(kMsgTempUpdate);
	fTempRunner = new BMessageRunner(BMessenger(this), &tempMsg, 2000000);
}


MainWindow::~MainWindow()
{
	delete fTempRunner;

	if (fSysBenchThread >= 0) {
		status_t result;
		wait_for_thread(fSysBenchThread, &result);
	}
}


void
MainWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgTempUpdate:
			_UpdateTemperature();
			break;

		case kMsgRunSysBench:
		{
			if (fSysBenchThread >= 0)
				break;

			fSysStatusLabel->SetText("Running system benchmarks...");
			fSysStatusLabel->SetHighColor(kYellow);
			fSysBenchThread = spawn_thread(_SysBenchThread,
				"sys_benchmark", B_NORMAL_PRIORITY, this);
			if (fSysBenchThread >= 0)
				resume_thread(fSysBenchThread);
			break;
		}

		case kMsgSysBenchDone:
		{
			_UpdateSysResults();
			fSysBenchThread = -1;

			if (fRunAllState == 0) {
				fSysStatusLabel->SetText(
					"Run All: system done, starting 2D...");
				fSysStatusLabel->SetHighColor(kYellow);
				fRunAllState = 1;
				PostMessage(kMsgRun2DBenchmark);
			} else {
				fSysStatusLabel->SetText(
					"System benchmark complete!");
				fSysStatusLabel->SetHighColor(kGreen);
			}
			break;
		}

		case kMsgRunBenchmark:
		{
			TeapotWindow* win = new TeapotWindow(BMessenger(this));
			win->Show();
			break;
		}

		case kMsgTeapotResult:
		{
			float fps = 0.0f;
			int32 teapots = 0;
			message->FindFloat("fps", &fps);
			message->FindInt32("teapots", &teapots);

			BString text;
			text.SetToFormat(
				"  Teapot 3D (OpenGL)  %8" B_PRId32 " FPS",
				(int32)fps);
			fTeapotResultLabel->SetText(text.String());
			fTeapotResultLabel->SetHighColor(kGreen);
			fTeapotResult.SetToFormat(
				"Teapot 3D: %" B_PRId32 " FPS (%" B_PRId32 " teapots)",
				(int32)fps, teapots);
			break;
		}

		case kMsgRun2DBenchmark:
		{
			Bench2DWindow* win = new Bench2DWindow(BMessenger(this));
			win->Show();
			break;
		}

		case kMsgBench2DResult:
		{
			float fillRPS = 0.0f;
			float nebulaOPS = 0.0f;
			float everythingOPS = 0.0f;
			message->FindFloat("ops_per_sec", 0, &fillRPS);
			message->FindFloat("ops_per_sec", 16, &nebulaOPS);
			message->FindFloat("ops_per_sec", 19, &everythingOPS);

			BString text;
			text.SetToFormat(
				"  2D Fill Rate        %8.0f ops/s",
				fillRPS);
			fBench2DResultLabel->SetText(text.String());
			fBench2DResultLabel->SetHighColor(kGreen);
			fBench2DResult.SetToFormat(
				"2D Bench: %.0f fill/s | nebula %.1f/s | extreme %.2f/s",
				fillRPS, nebulaOPS, everythingOPS);
			fBench2DFPS = fillRPS;
			if (fSysResults.valid)
				_UpdateSysResults();

			if (fRunAllState == 1) {
				fSysStatusLabel->SetText(
					"Run All: 2D done, starting GPU...");
				fRunAllState = 2;
				PostMessage(kMsgRunGpuBenchmark);
			}
			break;
		}

		case kMsgRunGpuBenchmark:
		{
			GpuBenchWindow* win = new GpuBenchWindow(BMessenger(this));
			win->Show();
			break;
		}

		case kMsgGpuBenchResult:
		{
			float gpuScore = 0.0f;
			BString gpuInfo;
			message->FindFloat("gpu_score", &gpuScore);
			message->FindString("gpu_info", &gpuInfo);

			BString text;
			text.SetToFormat(
				"  GPU Score (OpenGL)  %8.1f FPS",
				gpuScore);
			fGpuResultLabel->SetText(text.String());
			fGpuResultLabel->SetHighColor(kGreen);
			fGpuResult.SetToFormat(
				"GPU Bench: %.1f FPS (%s)",
				gpuScore, gpuInfo.String());

			// Hardware-vs-software comparison, when the GPU window ran it
			float speedup = 0.0f;
			if (message->FindFloat("gpu_speedup", &speedup) == B_OK
				&& speedup > 0.0f) {
				BString extra;
				extra.SetToFormat(" — hardware %.1fx faster than software",
					speedup);
				fGpuResult << extra;
			}
			fGpuScore = gpuScore;
			if (fSysResults.valid)
				_UpdateSysResults();

			if (fRunAllState == 2) {
				fSysStatusLabel->SetText(
					"Run All: GPU done, starting Vulkan...");
				fRunAllState = 3;
				PostMessage(kMsgRunVkBenchmark);
			}
			break;
		}

		case kMsgRunVkBenchmark:
		{
			VulkanBenchWindow* win = new VulkanBenchWindow(
				BMessenger(this));
			win->Show();
			break;
		}

		case kMsgVkBenchResult:
		{
			float vkScore = 0.0f;
			BString vkDevice, vkDriver;
			message->FindFloat("vk_score", &vkScore);
			message->FindString("vk_device", &vkDevice);
			message->FindString("vk_driver", &vkDriver);

			BString text;
			text.SetToFormat(
				"  Vulkan Compute      %8.1f pts",
				vkScore);
			fVkResultLabel->SetText(text.String());
			fVkResultLabel->SetHighColor(kGreen);
			fVkResult.SetToFormat(
				"Vulkan: %.1f pts (%s | %s)",
				vkScore, vkDevice.String(), vkDriver.String());
			fVkScore = vkScore;
			if (fSysResults.valid)
				_UpdateSysResults();

			if (fRunAllState == 3) {
				fRunAllState = -1;
				fSysStatusLabel->SetText("Run All complete!");
				fSysStatusLabel->SetHighColor(kGreen);
			}
			break;
		}

		case kMsgRunAllBench:
		{
			if (fSysBenchThread >= 0 || fRunAllState >= 0)
				break;

			fRunAllState = 0;
			fSysStatusLabel->SetText(
				"Run All: starting system benchmark...");
			fSysStatusLabel->SetHighColor(kYellow);
			PostMessage(kMsgRunSysBench);
			break;
		}

		case kMsgExportResults:
			_ExportResults();
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


bool
MainWindow::QuitRequested()
{
	be_app->PostMessage(B_QUIT_REQUESTED);
	return true;
}


void
MainWindow::_BuildLayout()
{
	BView* topView = new BView("top", B_WILL_DRAW);
	topView->SetViewColor(kDarkBg);

	BView* sysPanel = _CreateSystemInfoPanel();
	BView* resultsPanel = _CreateResultsPanel();

	// Buttons
	BButton* sysButton = new BButton("sysBtn",
		"System Bench", new BMessage(kMsgRunSysBench));
	BButton* bench2DButton = new BButton("bench2dBtn",
		"2D Bench", new BMessage(kMsgRun2DBenchmark));
	BButton* gpuButton = new BButton("gpuBtn",
		"GPU Bench", new BMessage(kMsgRunGpuBenchmark));
	BButton* teapotButton = new BButton("teapotBtn",
		"Teapot 3D", new BMessage(kMsgRunBenchmark));
	BButton* vulkanButton = new BButton("vulkanBtn",
		"Vulkan", new BMessage(kMsgRunVkBenchmark));
	BButton* runAllButton = new BButton("runAllBtn",
		"Run All", new BMessage(kMsgRunAllBench));
	BButton* exportButton = new BButton("exportBtn",
		"Export .md", new BMessage(kMsgExportResults));

	BLayoutBuilder::Group<>(topView, B_VERTICAL, 6)
		.SetInsets(12, 12, 12, 12)
		.Add(sysPanel)
		.Add(resultsPanel)
		.AddGlue()
		.Add(new BSeparatorView(B_HORIZONTAL))
		.AddGroup(B_HORIZONTAL, 6)
			.Add(runAllButton)
			.Add(sysButton)
			.Add(bench2DButton)
			.Add(gpuButton)
			.Add(teapotButton)
			.Add(vulkanButton)
			.AddGlue()
			.Add(exportButton)
		.End()
	.End();

	SetLayout(new BGroupLayout(B_VERTICAL));
	AddChild(topView);
}


BView*
MainWindow::_CreateSystemInfoPanel()
{
	BBox* box = new BBox("sysBox");
	box->SetViewColor(kDarkBg);
	box->SetLowColor(kDarkBg);

	BStringView* title = _MakeLabel("sysTitle", "System",
		kLabelWhite, 12.0f, true);
	title->SetViewColor(kDarkBg);
	title->SetLowColor(kDarkBg);
	box->SetLabel(title);

	BView* inner = new BView("sysInner", B_WILL_DRAW);
	inner->SetViewColor(kDarkBg);

	// System info
	BString sysDesc = SysBenchmark::GetSystemDescription();
	BString haikuVer = SysBenchmark::GetHaikuVersion();
	BString machineId;
	machineId.SetToFormat("Machine ID: %s",
		SysBenchmark::GetMachineId().String());

	fSystemLabel = _MakeLabel("system", sysDesc.String(), kWhite,
		11.0f, true);
	fHaikuLabel = _MakeLabel("haiku", haikuVer.String(), kCyan,
		11.0f);
	BStringView* machineIdLabel = _MakeLabel("machineId",
		machineId.String(), kYellow, 11.0f, true);
	fTempLabel = _MakeLabel("temp", "Temp: reading...", kGreen,
		11.0f, true);

	BLayoutBuilder::Group<>(inner, B_VERTICAL, 2)
		.SetInsets(8, 8, 8, 8)
		.Add(fSystemLabel)
		.Add(fHaikuLabel)
		.Add(machineIdLabel)
		.Add(fTempLabel)
	.End();

	box->AddChild(inner);
	return box;
}


BView*
MainWindow::_CreateResultsPanel()
{
	BBox* box = new BBox("resBox");
	box->SetViewColor(kDarkBg);
	box->SetLowColor(kDarkBg);

	BStringView* title = _MakeLabel("resTitle", "Benchmark Results",
		kLabelWhite, 12.0f, true);
	title->SetViewColor(kDarkBg);
	title->SetLowColor(kDarkBg);
	box->SetLabel(title);

	BView* inner = new BView("resInner", B_WILL_DRAW);
	inner->SetViewColor(kDarkBg);

	BFont monoFont(be_fixed_font);
	monoFont.SetSize(11.0f);

	static const rgb_color kMagenta = {220, 120, 180, 255};

	// Section headers
	BStringView* cpuHeader = _MakeLabel("cpuH", "CPU", kCyan,
		11.0f, true);
	BStringView* ramHeader = _MakeLabel("ramH", "Memory", kOrange,
		11.0f, true);
	BStringView* cacheHeader = _MakeLabel("cacheH", "Cache", kYellow,
		11.0f, true);
	BStringView* kernelHeader = _MakeLabel("kernH",
		"Kernel (Semaphores, Threads, Ports, Areas)", kPurple,
		11.0f, true);
	BStringView* msgHeader = _MakeLabel("msgH",
		"Messaging (BMessage, BLooper)", kMagenta, 11.0f, true);
	BStringView* graphicsHeader = _MakeLabel("gfxH",
		"Graphics", kGreen, 11.0f, true);

	// Create test labels - use fixed-width placeholder so layout
	// doesn't resize when results appear
	for (int32 i = 0; i < SysBenchmark::kNumTests; i++) {
		BString name;
		name.SetToFormat("sysTest%" B_PRId32, i);
		BString text;
		text.SetToFormat("  %-22s       --       ", SysBenchmark::TestName(i));
		fSysTestLabels[i] = _MakeLabel(name.String(), text.String(),
			kGray, 11.0f);
		fSysTestLabels[i]->SetFont(&monoFont);
		fSysTestLabels[i]->SetExplicitMinSize(
			BSize(fSysTestLabels[i]->StringWidth(text.String()) + 20,
				B_SIZE_UNSET));
	}

	fSysStatusLabel = _MakeLabel("sysStatus",
		"Press 'System Bench' to start", kGray, 10.0f);

	// Graphics results — same format as system bench:
	//   "  %-22s  %8s unit"
	fTeapotResultLabel = _MakeLabel("teapot",
		"  Teapot 3D (OpenGL)        --       ", kGray, 11.0f);
	fTeapotResultLabel->SetFont(&monoFont);
	fTeapotResultLabel->SetExplicitMinSize(
		BSize(fTeapotResultLabel->StringWidth(
			"  Teapot 3D (OpenGL)    99999 FPS   "), B_SIZE_UNSET));
	fBench2DResultLabel = _MakeLabel("bench2d",
		"  2D Fill Rate              --       ", kGray, 11.0f);
	fBench2DResultLabel->SetFont(&monoFont);
	fBench2DResultLabel->SetExplicitMinSize(
		BSize(fBench2DResultLabel->StringWidth(
			"  2D Fill Rate          99999 ops/s "), B_SIZE_UNSET));
	fGpuResultLabel = _MakeLabel("gpu",
		"  GPU Score (OpenGL)        --       ", kGray, 11.0f);
	fGpuResultLabel->SetFont(&monoFont);
	fGpuResultLabel->SetExplicitMinSize(
		BSize(fGpuResultLabel->StringWidth(
			"  GPU Score (OpenGL)    999.9 FPS   "), B_SIZE_UNSET));
	fVkResultLabel = _MakeLabel("vk",
		"  Vulkan Compute            --       ", kGray, 11.0f);
	fVkResultLabel->SetFont(&monoFont);
	fVkResultLabel->SetExplicitMinSize(
		BSize(fVkResultLabel->StringWidth(
			"  Vulkan Compute        999.9 pts   "), B_SIZE_UNSET));

	// Score labels (SPEC-style, baseline = 1000)
	BStringView* scoreHeader = _MakeLabel("scoreH",
		"Score (baseline = 1000)", kGreen, 11.0f, true);
	{
		BFont scoreMono(be_fixed_font);
		scoreMono.SetSize(11.0f);
		for (int32 i = 0; i < kNumScoreCategoriesAll; i++) {
			BString name;
			name.SetToFormat("score%" B_PRId32, i);
			BString text;
			text.SetToFormat("  %-22s       --",
				kScoreCategoryNamesAll[i]);
			fScoreLabels[i] = _MakeLabel(name.String(),
				text.String(), kGray, 11.0f);
			fScoreLabels[i]->SetFont(&scoreMono);
		}
		// Overall label (index = kNumScoreCategoriesAll = 8)
		fScoreLabels[kNumScoreCategoriesAll] = _MakeLabel("scoreTotal",
			"  OVERALL                    --", kGreen, 11.0f, true);
		BFont scoreBold(be_fixed_font);
		scoreBold.SetSize(11.0f);
		scoreBold.SetFace(B_BOLD_FACE);
		fScoreLabels[kNumScoreCategoriesAll]->SetFont(&scoreBold);
	}

	BLayoutBuilder::Group<>(inner, B_HORIZONTAL, 10)
		.SetInsets(8, 8, 8, 8)
		// Left column: CPU, Memory, Cache, Graphics
		.AddGroup(B_VERTICAL, 1)
			.Add(cpuHeader)
			.Add(fSysTestLabels[0])
			.Add(fSysTestLabels[1])
			.Add(fSysTestLabels[2])
			.Add(new BSeparatorView(B_HORIZONTAL))
			.Add(ramHeader)
			.Add(fSysTestLabels[3])
			.Add(fSysTestLabels[4])
			.Add(fSysTestLabels[5])
			.Add(fSysTestLabels[6])
			.Add(new BSeparatorView(B_HORIZONTAL))
			.Add(cacheHeader)
			.Add(fSysTestLabels[7])
			.Add(fSysTestLabels[8])
			.Add(fSysTestLabels[9])
			.Add(new BSeparatorView(B_HORIZONTAL))
			.Add(graphicsHeader)
			.Add(fTeapotResultLabel)
			.Add(fBench2DResultLabel)
			.Add(fGpuResultLabel)
			.Add(fVkResultLabel)
			.AddGlue()
		.End()
		.Add(new BSeparatorView(B_VERTICAL))
		// Right column: Kernel, Messaging
		.AddGroup(B_VERTICAL, 1)
			.Add(kernelHeader)
			.Add(fSysTestLabels[10])
			.Add(fSysTestLabels[11])
			.Add(fSysTestLabels[12])
			.Add(fSysTestLabels[13])
			.Add(fSysTestLabels[14])
			.Add(fSysTestLabels[15])
			.Add(fSysTestLabels[16])
			.Add(fSysTestLabels[17])
			.Add(new BSeparatorView(B_HORIZONTAL))
			.Add(msgHeader)
			.Add(fSysTestLabels[18])
			.Add(fSysTestLabels[19])
			.Add(new BSeparatorView(B_HORIZONTAL))
			.Add(scoreHeader)
			.Add(fScoreLabels[0])
			.Add(fScoreLabels[1])
			.Add(fScoreLabels[2])
			.Add(fScoreLabels[3])
			.Add(fScoreLabels[4])
			.Add(fScoreLabels[5])
			.Add(fScoreLabels[6])
			.Add(fScoreLabels[7])
			.Add(fScoreLabels[kNumScoreCategoriesAll])
			.Add(new BSeparatorView(B_HORIZONTAL))
			.Add(fSysStatusLabel)
			.AddGlue()
		.End()
	.End();

	box->AddChild(inner);
	return box;
}


void
MainWindow::_UpdateTemperature()
{
	const char* zoneNames[] = {"CPU", "GPU"};
	BString tempText("Temp:");
	float maxTemp = 0.0f;
	bool found = false;

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
			const char* label = (i < 2) ? zoneNames[i] : "Zone";
			BString zone;
			if (i >= 2)
				zone.SetToFormat(" %s%" B_PRId32 ": %.0f\xC2\xB0""C",
					label, i, current);
			else
				zone.SetToFormat(" %s: %.0f\xC2\xB0""C", label, current);
			tempText.Append(zone);

			if (current > maxTemp)
				maxTemp = current;
			found = true;
		}
	}

	if (found) {
		rgb_color tempColor = kGreen;
		if (maxTemp >= 80.0f)
			tempColor = (rgb_color){255, 80, 80, 255};
		else if (maxTemp >= 65.0f)
			tempColor = (rgb_color){230, 200, 50, 255};

		fTempLabel->SetHighColor(tempColor);
		fTempLabel->SetText(tempText.String());
	} else {
		fTempLabel->SetHighColor(kGray);
		fTempLabel->SetText("Temp: no sensors found");
	}
}


void
MainWindow::_UpdateSysResults()
{
	if (!fSysResults.valid)
		return;

	BFont monoFont(be_fixed_font);
	monoFont.SetSize(11.0f);

	static const rgb_color kMagenta = {220, 120, 180, 255};

	const rgb_color sectionColors[] = {
		kCyan, kCyan, kCyan,								// CPU
		kOrange, kOrange, kOrange, kOrange,					// RAM
		kYellow, kYellow, kYellow,							// Cache
		kPurple, kPurple, kPurple, kPurple, kPurple,		// Kernel
			kPurple, kPurple, kPurple,
		kMagenta, kMagenta									// Messaging
	};

	const char* units[] = {
		"MOPS", "MFLOPS", "x speedup",
		"MB/s", "MB/s", "MB/s", "ns",
		"MB/s", "MB/s", "MB/s",
		"kOPS", "kOPS", "kOPS", "kOPS", "kOPS",
		"kOPS", "kOPS", "ns",
		"kOPS", "kOPS"
	};

	for (int32 i = 0; i < SysBenchmark::kNumTests; i++) {
		float val = SysBenchmark::ResultValue(fSysResults, i);
		float sd = fSysResults.stddev[i];
		BString text;
		if (val < 0.0f) {
			text.SetToFormat("  %-22s      N/A",
				SysBenchmark::TestName(i));
		} else if (val >= 10000.0f) {
			text.SetToFormat("  %-22s %7.0f\xC2\xB1%-4.0f %s",
				SysBenchmark::TestName(i), val, sd, units[i]);
		} else if (i == 2) {
			text.SetToFormat("  %-22s %7.2f\xC2\xB1%-4.2f %s",
				SysBenchmark::TestName(i), val, sd, units[i]);
		} else {
			text.SetToFormat("  %-22s %7.1f\xC2\xB1%-4.1f %s",
				SysBenchmark::TestName(i), val, sd, units[i]);
		}
		fSysTestLabels[i]->SetText(text.String());
		fSysTestLabels[i]->SetHighColor(sectionColors[i]);
	}

	// Compute and display SPEC-style score
	ScoreResult score = ComputeScore(fSysResults,
		fBench2DFPS, fGpuScore, fVkScore);
	if (score.valid) {
		static const rgb_color catColors[] = {
			kCyan, kOrange, kYellow, kPurple,
			{220, 120, 180, 255},	// Messaging
			kGreen,					// 2D Graphics
			kGreen,					// GPU
			kGreen					// Vulkan
		};
		for (int32 i = 0; i < kNumScoreCategoriesAll; i++) {
			BString text;
			if (score.categoryScore[i] > 0.0f) {
				text.SetToFormat("  %-22s  %7.0f",
					kScoreCategoryNamesAll[i],
					score.categoryScore[i]);
				fScoreLabels[i]->SetHighColor(catColors[i]);
			} else {
				text.SetToFormat("  %-22s       --",
					kScoreCategoryNamesAll[i]);
				fScoreLabels[i]->SetHighColor(kGray);
			}
			fScoreLabels[i]->SetText(text.String());
		}
		BString text;
		text.SetToFormat("  OVERALL               %7.0f",
			score.overall);
		fScoreLabels[kNumScoreCategoriesAll]->SetText(text.String());
		fScoreLabels[kNumScoreCategoriesAll]->SetHighColor(kGreen);
	}
}


/*static*/ int32
MainWindow::_TestCallback(int32 test, void* cookie)
{
	MainWindow* window = static_cast<MainWindow*>(cookie);
	window->fCurrentSysTest = test;

	// Update UI from the window thread
	if (window->LockWithTimeout(100000) == B_OK) {
		BString status;
		status.SetToFormat("Running: %s (%" B_PRId32 "/%d) "
			"\xC3\x97%" B_PRId32 " runs...",
			SysBenchmark::TestName(test), test + 1,
			SysBenchmark::kNumTests, (int32)kBenchRuns);
		window->fSysStatusLabel->SetText(status.String());

		// Mark current test as running
		BString text;
		text.SetToFormat("  %-22s  running...",
			SysBenchmark::TestName(test));
		window->fSysTestLabels[test]->SetText(text.String());
		window->fSysTestLabels[test]->SetHighColor(kYellow);

		window->Unlock();
	}

	return 0;
}


/*static*/ int32
MainWindow::_SysBenchThread(void* data)
{
	MainWindow* window = static_cast<MainWindow*>(data);

	window->fSysResults = SysBenchmark::Run(_TestCallback, window);

	// Notify main window
	BMessenger(window).SendMessage(kMsgSysBenchDone);
	return 0;
}


void
MainWindow::_ExportResults()
{
	// Generate timestamp for filename
	time_t now = time(NULL);
	struct tm* t = localtime(&now);
	char timestamp[64];
	strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H-%M-%S", t);

	// Save to Desktop
	BPath path;
	find_directory(B_DESKTOP_DIRECTORY, &path);
	BString filePath;
	filePath.SetToFormat("%s/HaikuBench_%s.md", path.Path(), timestamp);

	FILE* f = fopen(filePath.String(), "w");
	if (f == NULL) {
		BAlert* alert = new BAlert("Error",
			"Could not create export file.", "OK");
		alert->Go();
		return;
	}

	// Header
	BString machineId = SysBenchmark::GetMachineId();
	fprintf(f, "# HaikuBench %s Results\n\n", HAIKUBENCH_VERSION);

	char dateStr[64];
	strftime(dateStr, sizeof(dateStr), "%Y-%m-%d %H:%M:%S", t);
	fprintf(f, "**Date:** %s\n", dateStr);
	fprintf(f, "**Machine ID:** `%s`\n", machineId.String());
	fprintf(f, "**Version:** %s\n\n", HAIKUBENCH_VERSION);

	// System info
	fprintf(f, "## System\n\n");
	fprintf(f, "- **Hardware:** %s\n",
		SysBenchmark::GetSystemDescription().String());
	fprintf(f, "- **OS:** %s\n\n",
		SysBenchmark::GetHaikuVersion().String());

	// System benchmark results
	if (fSysResults.valid) {
		const char* units[] = {
			"MOPS", "MFLOPS", "x speedup",
			"MB/s", "MB/s", "MB/s", "ns",
			"MB/s", "MB/s", "MB/s",
			"kOPS", "kOPS", "kOPS", "kOPS", "kOPS",
			"kOPS", "kOPS", "ns",
			"kOPS", "kOPS"
		};

		const char* sections[] = {
			"CPU", "CPU", "CPU",
			"Memory", "Memory", "Memory", "Memory",
			"Cache", "Cache", "Cache",
			"Kernel", "Kernel", "Kernel", "Kernel", "Kernel",
			"Kernel", "Kernel", "Kernel",
			"Messaging", "Messaging"
		};

		fprintf(f, "## System Benchmark\n\n");
		fprintf(f, "_Per-test adaptive sampling: %d to %d runs, "
			"stop when CoV < 2%%._\n\n",
			(int)kBenchRuns, 10);
		fprintf(f, "| Section | Test | Mean | \xC2\xB1 StdDev | p50 | p95 | "
			"CoV | n | Unit |\n");
		fprintf(f, "|---------|------|-----:|--------:|----:|----:|----:|"
			"--:|------|\n");

		for (int32 i = 0; i < SysBenchmark::kNumTests; i++) {
			float val = SysBenchmark::ResultValue(fSysResults, i);
			float sd = fSysResults.stddev[i];
			const BenchStats& bs = fSysResults.stats[i];
			if (val < 0.0f) {
				fprintf(f, "| %s | %s | N/A | | | | | | %s |\n",
					sections[i], SysBenchmark::TestName(i), units[i]);
				continue;
			}
			const char* fmt;
			if (val >= 10000.0f)
				fmt = "| %s | %s | %.0f | %.0f | %.0f | %.0f | %.1f%% | %d"
					" | %s |\n";
			else if (i == 2)
				fmt = "| %s | %s | %.2f | %.2f | %.2f | %.2f | %.1f%% | %d"
					" | %s |\n";
			else
				fmt = "| %s | %s | %.1f | %.1f | %.1f | %.1f | %.1f%% | %d"
					" | %s |\n";
			fprintf(f, fmt,
				sections[i], SysBenchmark::TestName(i),
				val, sd, bs.median, bs.p95,
				bs.cov * 100.0f, (int)bs.nSamples, units[i]);
		}

		fprintf(f, "\n");
	} else {
		fprintf(f, "## System Benchmark\n\n");
		fprintf(f, "*Not yet run.*\n\n");
	}

	// Graphics benchmarks
	fprintf(f, "## Graphics Benchmark\n\n");
	if (fTeapotResult.Length() > 0)
		fprintf(f, "- **%s**\n", fTeapotResult.String());
	else
		fprintf(f, "- 3D Teapot: *not yet run*\n");

	if (fBench2DResult.Length() > 0)
		fprintf(f, "- **%s**\n", fBench2DResult.String());
	else
		fprintf(f, "- 2D Bench: *not yet run*\n");

	if (fGpuResult.Length() > 0)
		fprintf(f, "- **%s**\n", fGpuResult.String());
	else
		fprintf(f, "- GPU Bench: *not yet run*\n");

	if (fVkResult.Length() > 0)
		fprintf(f, "- **%s**\n", fVkResult.String());
	else
		fprintf(f, "- Vulkan: *not yet run*\n");

	// SPEC-style score (geometric mean of ratios vs baseline = 1000)
	if (fSysResults.valid) {
		ScoreResult score = ComputeScore(fSysResults,
			fBench2DFPS, fGpuScore, fVkScore);
		if (score.valid) {
			fprintf(f, "\n## Score (baseline = 1000)\n\n");
			fprintf(f, "_Geometric mean of per-test ratios vs reference "
				"system. 1000 = identical to baseline._\n\n");
			fprintf(f, "| Category | Score |\n");
			fprintf(f, "|----------|------:|\n");
			for (int32 i = 0; i < kNumScoreCategoriesAll; i++) {
				if (score.categoryScore[i] > 0.0f)
					fprintf(f, "| %s | %.0f |\n",
						kScoreCategoryNamesAll[i],
						score.categoryScore[i]);
				else
					fprintf(f, "| %s | -- |\n",
						kScoreCategoryNamesAll[i]);
			}
			fprintf(f, "| **OVERALL** | **%.0f** |\n",
				score.overall);
			fprintf(f, "\n");
		}
	}

	fprintf(f, "---\n*Generated by HaikuBench — Machine `%s`*\n",
		machineId.String());

	fclose(f);

	// Export JSON for machine parsing
	BString jsonPath;
	jsonPath.SetToFormat("%s/HaikuBench_%s.json", path.Path(), timestamp);

	FILE* j = fopen(jsonPath.String(), "w");
	if (j != NULL) {
		fprintf(j, "{\n");
		fprintf(j, "  \"format_version\": 1,\n");
		fprintf(j, "  \"app_version\": \"%s\",\n", HAIKUBENCH_VERSION);
		fprintf(j, "  \"date\": \"%s\",\n", dateStr);
		fprintf(j, "  \"machine_id\": \"%s\",\n", machineId.String());
		fprintf(j, "  \"hardware\": \"%s\",\n",
			SysBenchmark::GetSystemDescription().String());
		fprintf(j, "  \"os\": \"%s\",\n",
			SysBenchmark::GetHaikuVersion().String());

		// System benchmark
		fprintf(j, "  \"system_bench\": {\n");
		fprintf(j, "    \"valid\": %s,\n",
			fSysResults.valid ? "true" : "false");
		fprintf(j, "    \"runs\": %d,\n", (int)fSysResults.runs);
		if (fSysResults.valid) {
			static const char* jsonKeys[] = {
				"cpu_integer_mops", "cpu_float_mflops",
				"cpu_multithread_x",
				"ram_seq_read_mbs", "ram_seq_write_mbs",
				"ram_copy_mbs", "ram_latency_ns",
				"cache_l1_mbs", "cache_l2_mbs", "cache_l3_mbs",
				"sem_create_delete_kops", "sem_acquire_release_kops",
				"sem_contention_kops", "thread_spawn_kops",
				"port_send_recv_kops", "area_create_delete_kops",
				"atomic_ops_kops", "syscall_overhead_ns",
				"bmessage_flatten_kops", "blooper_messaging_kops"
			};
			fprintf(j, "    \"results\": {\n");
			for (int32 i = 0; i < SysBenchmark::kNumTests; i++) {
				float val = SysBenchmark::ResultValue(fSysResults, i);
				float sd = fSysResults.stddev[i];
				const BenchStats& bs = fSysResults.stats[i];
				fprintf(j,
					"      \"%s\": {"
					"\"mean\": %.4f, "
					"\"stddev\": %.4f, "
					"\"median\": %.4f, "
					"\"p95\": %.4f, "
					"\"min\": %.4f, "
					"\"max\": %.4f, "
					"\"cov\": %.4f, "
					"\"trimmed_mean\": %.4f, "
					"\"n_samples\": %d, "
					"\"n_dropped\": %d"
					"}%s\n",
					jsonKeys[i], val, sd,
					bs.median, bs.p95, bs.min, bs.max,
					bs.cov, bs.trimmedMean,
					(int)bs.nSamples, (int)bs.nDropped,
					(i < SysBenchmark::kNumTests - 1) ? "," : "");
			}
			fprintf(j, "    }\n");
		}
		fprintf(j, "  },\n");

		// Graphics
		fprintf(j, "  \"graphics\": {\n");
		fprintf(j, "    \"teapot\": %s,\n",
			fTeapotResult.Length() > 0
				? BString().SetToFormat("\"%s\"",
					fTeapotResult.String()).String()
				: "null");
		fprintf(j, "    \"bench_2d\": %s,\n",
			fBench2DResult.Length() > 0
				? BString().SetToFormat("\"%s\"",
					fBench2DResult.String()).String()
				: "null");
		fprintf(j, "    \"gpu_opengl\": %s,\n",
			fGpuResult.Length() > 0
				? BString().SetToFormat("\"%s\"",
					fGpuResult.String()).String()
				: "null");
		fprintf(j, "    \"vulkan\": %s\n",
			fVkResult.Length() > 0
				? BString().SetToFormat("\"%s\"",
					fVkResult.String()).String()
				: "null");
		fprintf(j, "  },\n");

		// Score
		fprintf(j, "  \"score\": {\n");
		if (fSysResults.valid) {
			ScoreResult score = ComputeScore(fSysResults,
				fBench2DFPS, fGpuScore, fVkScore);
			if (score.valid) {
				static const char* jsonCatKeys[] = {
					"cpu", "memory", "cache", "kernel", "messaging",
					"graphics_2d", "gpu_opengl", "vulkan"
				};
				fprintf(j, "    \"method\": \"geometric_mean_vs_baseline\",\n");
				fprintf(j, "    \"baseline\": 1000,\n");
				for (int32 i = 0; i < kNumScoreCategoriesAll; i++) {
					if (score.categoryScore[i] > 0.0f)
						fprintf(j, "    \"%s\": %.1f,\n",
							jsonCatKeys[i], score.categoryScore[i]);
					else
						fprintf(j, "    \"%s\": null,\n",
							jsonCatKeys[i]);
				}
				fprintf(j, "    \"overall\": %.1f\n", score.overall);
			}
		}
		fprintf(j, "  }\n");

		fprintf(j, "}\n");
		fclose(j);
	}

	// Show confirmation
	BString msg;
	msg.SetToFormat("Results exported to:\n%s\n%s",
		filePath.String(), jsonPath.String());
	BAlert* alert = new BAlert("Export", msg.String(), "OK");
	alert->Go();
}
