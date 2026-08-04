/*
 * Copyright 2025 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 */


#include <string.h>

#include <Application.h>

#include "GpuBenchWindow.h"
#include "MainWindow.h"
#include "SplashWindow.h"


class HaikuBenchApp : public BApplication {
public:
								HaikuBenchApp();

	virtual	void				ReadyToRun();
};


HaikuBenchApp::HaikuBenchApp()
	:
	BApplication("application/x-vnd.HaikuBench")
{
}


void
HaikuBenchApp::ReadyToRun()
{
	// Build the main window but keep it hidden; the splash reveals it when it
	// finishes (or is skipped). If splashes are disabled, Launch() shows it
	// immediately.
	MainWindow* window = new MainWindow();
	SplashWindow::Launch(window);
}


int
main(int argc, char** argv)
{
	// Child mode: software comparison pass for the GPU benchmark
	// (spawned by GpuBenchPanel with HGL_SOFTWARE=1).
	if (argc >= 3 && strcmp(argv[1], "--gpu-sw-pass") == 0)
		return RunGpuSoftwarePass(argv[2]);

	HaikuBenchApp app;
	app.Run();

	return 0;
}
