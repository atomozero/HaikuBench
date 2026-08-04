/*
 * Copyright 2025-2026 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef SETTINGS_H
#define SETTINGS_H


#include <SupportDefs.h>

class BMessage;
class BPath;


// Tiny persistent settings store: a flattened BMessage kept in the user's
// settings directory. Reads and writes are self-contained (no long-lived
// state), so it is safe to call from any thread — including the app entry
// point before any window exists.
class Settings {
public:
	static	bool		ShowSplash();		// default true
	static	void		SetShowSplash(bool show);

private:
	static	status_t	_Load(BMessage& into);
	static	status_t	_Save(const BMessage& from);
	static	void		_Path(BPath& path);
};


#endif // SETTINGS_H
