/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef PES_LAUNCHER_SETTINGS_ONLINE_H
#define PES_LAUNCHER_SETTINGS_ONLINE_H

#include <windows.h>

/* Creates the Online tab under parent: the reference's "Coming Soon" panel. The port has no online
 * play, so the tab stores nothing. Hidden until the tab is shown. */
void SettingsOnlineCreate(HWND parent, HFONT font);
/* Shows or hides the tab (the tab control switches pages). */
void SettingsOnlineShow(int visible);

#endif
