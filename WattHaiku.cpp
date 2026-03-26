/*
 * Copyright 2025 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 */


#include <Application.h>

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
main()
{
	HaikuBenchApp app;
	app.Run();

	return 0;
}
