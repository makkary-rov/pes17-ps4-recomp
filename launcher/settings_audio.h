/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef PES_LAUNCHER_SETTINGS_AUDIO_H
#define PES_LAUNCHER_SETTINGS_AUDIO_H

#include <windows.h>

/* Creates the Audio tab under parent, with the given font, and loads the saved audio buffer.
 * Hidden until the tab is shown. */
void SettingsAudioCreate(HWND parent, HFONT font);
/* Shows or hides the tab (the tab control switches pages). */
void SettingsAudioShow(int visible);
/* The audio buffer chosen on the tab, 1..8 (saved as audio_buffer; no effect on the sound yet). */
int SettingsAudioBuffer(void);

#endif
