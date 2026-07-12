/*
 * Copyright 2025 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 */


#include <string.h>

#include <Application.h>

#include "GpuBenchWindow.h"
#include "MainWindow.h"


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
	MainWindow* window = new MainWindow();
	window->Show();
}


int
main(int argc, char** argv)
{
	// Child mode: software comparison pass for the GPU benchmark
	// (spawned by GpuBenchWindow with HGL_SOFTWARE=1).
	if (argc >= 3 && strcmp(argv[1], "--gpu-sw-pass") == 0)
		return RunGpuSoftwarePass(argv[2]);

	HaikuBenchApp app;
	app.Run();

	return 0;
}
