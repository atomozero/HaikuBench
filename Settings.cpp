/*
 * Copyright 2025-2026 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 */


#include "Settings.h"

#include <File.h>
#include <FindDirectory.h>
#include <Message.h>
#include <Path.h>


static const char* kSettingsFile = "HaikuBench_settings";
static const char* kShowSplashKey = "show_splash";


void
Settings::_Path(BPath& path)
{
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK)
		return;
	path.Append(kSettingsFile);
}


status_t
Settings::_Load(BMessage& into)
{
	BPath path;
	_Path(path);
	if (path.Path() == NULL)
		return B_ERROR;

	BFile file(path.Path(), B_READ_ONLY);
	if (file.InitCheck() != B_OK)
		return file.InitCheck();

	return into.Unflatten(&file);
}


status_t
Settings::_Save(const BMessage& from)
{
	BPath path;
	_Path(path);
	if (path.Path() == NULL)
		return B_ERROR;

	BFile file(path.Path(),
		B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() != B_OK)
		return file.InitCheck();

	return from.Flatten(&file);
}


bool
Settings::ShowSplash()
{
	BMessage settings;
	if (_Load(settings) != B_OK)
		return true;			// no settings yet -> default on

	bool show;
	if (settings.FindBool(kShowSplashKey, &show) != B_OK)
		return true;
	return show;
}


void
Settings::SetShowSplash(bool show)
{
	// Preserve any other keys that may already be stored.
	BMessage settings;
	_Load(settings);

	if (settings.ReplaceBool(kShowSplashKey, show) != B_OK)
		settings.AddBool(kShowSplashKey, show);

	_Save(settings);
}
