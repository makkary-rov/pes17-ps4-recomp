/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef PES_LAUNCHER_SETTINGS_DISPLAY_H
#define PES_LAUNCHER_SETTINGS_DISPLAY_H

#include <windows.h>
#include "settings_model.h"

/* Creates the Display tab's controls under parent (the settings window), with the given font. */
void SettingsDisplayCreate(HWND parent, HFONT font);
/* Shows or hides the tab's controls (the tab control switches pages). */
void SettingsDisplayShow(int visible);
/* Puts a model's values on the controls, and reads them back. */
void SettingsDisplaySetValues(PesDisplaySettings s);
PesDisplaySettings SettingsDisplayGetValues(void);

#endif
