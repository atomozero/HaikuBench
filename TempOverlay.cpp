/*
 * Copyright 2025 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 */


#include "TempOverlay.h"

#include <GL/gl.h>
#include <GL/glut.h>
#include <OS.h>

#include <stdio.h>
#include <string.h>


float TempOverlay::sTemp[kMaxZones] = {};
float TempOverlay::sCritical[kMaxZones] = {};
int32 TempOverlay::sZoneCount = 0;
bigtime_t TempOverlay::sLastReadTime = 0;
std::atomic_flag TempOverlay::sLock = ATOMIC_FLAG_INIT;


void
TempOverlay::ReadTemperatures()
{
	// Throttle reads to once per second
	bigtime_t now = system_time();
	if (now - sLastReadTime < 1000000 && sZoneCount > 0)
		return;

	// Acquire spinlock to protect static state
	while (sLock.test_and_set(std::memory_order_acquire))
		;
	sLastReadTime = now;

	sZoneCount = 0;

	for (int32 i = 0; i < kMaxZones; i++) {
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
		float critical = -1.0f;

		const char* critPtr = strstr(buffer, "Critical Temperature:");
		if (critPtr != NULL)
			sscanf(critPtr, "Critical Temperature: %f", &critical);

		const char* curPtr = strstr(buffer, "Current Temperature:");
		if (curPtr != NULL)
			sscanf(curPtr, "Current Temperature: %f", &current);

		if (current >= 0.0f) {
			sTemp[i] = current;
			sCritical[i] = critical;
			sZoneCount = i + 1;
		}
	}

	sLock.clear(std::memory_order_release);
}


void
TempOverlay::DrawOverlay(float viewWidth, float viewHeight)
{
	if (sZoneCount <= 0)
		return;

	// Acquire spinlock to read consistent temperature data
	while (sLock.test_and_set(std::memory_order_acquire))
		;

	// Save GL state
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0, viewWidth, 0, viewHeight, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Build overlay text lines
	const char* zoneNames[] = {"CPU", "GPU"};
	char lines[kMaxZones][64];
	int32 numLines = 0;

	for (int32 i = 0; i < sZoneCount && i < kMaxZones; i++) {
		const char* label = (i < 2) ? zoneNames[i] : "Zone";
		if (i >= 2) {
			snprintf(lines[numLines], sizeof(lines[numLines]),
				"%s%" B_PRId32 ": %.0f°C / %.0f°C",
				label, i, sTemp[i], sCritical[i]);
		} else {
			snprintf(lines[numLines], sizeof(lines[numLines]),
				"%s: %.0f%cC / %.0f%cC",
				label, sTemp[i], 0xB0, sCritical[i], 0xB0);
		}
		numLines++;
	}

	// Calculate overlay dimensions
	float lineHeight = 16.0f;
	float padding = 6.0f;
	float overlayHeight = numLines * lineHeight + padding * 2.0f;
	float overlayWidth = 160.0f;
	float posX = viewWidth - overlayWidth - 8.0f;
	float posY = 8.0f;

	// Draw semi-transparent background
	_DrawBackground(posX, posY, overlayWidth, overlayHeight);

	// Draw temperature text
	for (int32 i = 0; i < numLines; i++) {
		float temp = sTemp[i];

		// Color based on temperature
		if (temp >= 80.0f)
			glColor4f(1.0f, 0.3f, 0.3f, 1.0f);		// red
		else if (temp >= 65.0f)
			glColor4f(0.9f, 0.8f, 0.2f, 1.0f);		// yellow
		else
			glColor4f(0.22f, 0.83f, 0.33f, 1.0f);		// green

		float textY = posY + overlayHeight - padding
			- (float)i * lineHeight - 12.0f;
		_DrawString(lines[i], posX + padding, textY);
	}

	// Restore GL state
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glPopAttrib();

	sLock.clear(std::memory_order_release);
}


void
TempOverlay::_DrawString(const char* str, float x, float y)
{
	glRasterPos2f(x, y);
	while (*str != '\0') {
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *str);
		str++;
	}
}


void
TempOverlay::_DrawBackground(float x, float y, float width, float height)
{
	glColor4f(0.0f, 0.0f, 0.0f, 0.65f);
	glBegin(GL_QUADS);
	glVertex2f(x, y);
	glVertex2f(x + width, y);
	glVertex2f(x + width, y + height);
	glVertex2f(x, y + height);
	glEnd();

	// Thin border
	glColor4f(0.3f, 0.3f, 0.3f, 0.8f);
	glLineWidth(1.0f);
	glBegin(GL_LINE_LOOP);
	glVertex2f(x, y);
	glVertex2f(x + width, y);
	glVertex2f(x + width, y + height);
	glVertex2f(x, y + height);
	glEnd();
}
