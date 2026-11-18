/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef PES_LAUNCHER_SETTINGS_CONTROLLER_H
#define PES_LAUNCHER_SETTINGS_CONTROLLER_H

#include <windows.h>
#include "settings_pad.h"

/* Creates the Controller tab under parent, with the given font. Hidden until the tab is shown. */
void SettingsControllerCreate(HWND parent, HFONT font);
/* Shows or hides the tab (the tab control switches pages). */
void SettingsControllerShow(int visible);
/* Returns TRUE if the message was taken by the controller capture (Esc cancels a waiting button). */
BOOL SettingsControllerFilter(const MSG *msg);
/* The gamepad bindings (SDL names; "" = unbound). */
void SettingsControllerGet(char pads[PES_PAD_SLOTS][PES_PAD_NAME_MAX]);
void SettingsControllerSet(const char pads[PES_PAD_SLOTS][PES_PAD_NAME_MAX]);

#endif
