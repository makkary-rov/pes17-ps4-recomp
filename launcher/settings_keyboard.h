/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef PES_LAUNCHER_SETTINGS_KEYBOARD_H
#define PES_LAUNCHER_SETTINGS_KEYBOARD_H

#include <windows.h>
#include "settings_keys.h"

/* Creates the Keyboard tab under parent, with the given font. Hidden until the tab is shown. */
void SettingsKeyboardCreate(HWND parent, HFONT font);
/* Shows or hides the tab (the tab control switches pages). */
void SettingsKeyboardShow(int visible);
/* Returns TRUE if the message was taken by a key capture (a binding button waiting for a key).
 * Call it in the message loop before dispatching. */
BOOL SettingsKeyboardFilter(const MSG *msg);
/* The 24 bindings (Windows virtual-key codes, 0 = unbound). */
void SettingsKeyboardGet(int keys[PES_PAD_SLOTS]);
void SettingsKeyboardSet(const int keys[PES_PAD_SLOTS]);

#endif
