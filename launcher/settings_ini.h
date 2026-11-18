/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Keyboard bindings in the port's settings file (pes17recomp.ini, or PES_CONFIG): the key.<input>=
 * lines that the game reads at start (runtime_pad.c). Every other line of the file is kept as it is.
 * Plain C with stdio only, so it is tested on every platform. */
#ifndef PES_LAUNCHER_SETTINGS_INI_H
#define PES_LAUNCHER_SETTINGS_INI_H

#include "settings_keys.h"
#include "settings_pad.h"

/* The settings file the game and the settings window share: PES_CONFIG, else pes17recomp.ini. */
const char *PesConfigPath(void);

/* Writes the bindings: key.<input>= for the 24 slots and pad.<input>= for the bindable slots
 * (replacing those lines; an unbound slot is written empty). Other lines, comments included, are kept
 * in order. The file is replaced through a temporary file. Returns 1 on success, 0 on failure. */
int PesIniSaveBindings(const char *path, const int keys[PES_PAD_SLOTS], const char pads[PES_PAD_SLOTS][PES_PAD_NAME_MAX]);

/* The gamepad input method line (pad_input=xinput|directinput). Returns 1 if the file has it. */
int PesIniSavePadMethod(const char *path, int method);
int PesIniLoadPadMethod(const char *path, int *method);

/* One whole-number setting, key=value (for example audio_buffer=3). Save replaces the key's line or
 * appends it; load returns 1 if the file has a number for the key, else 0 (value unchanged). */
int PesIniSaveInt(const char *path, const char *key, int value);
int PesIniLoadInt(const char *path, const char *key, int *value);

/* Reads the bindings. Returns bit 1 if the file had key lines (keys set), bit 2 if it had pad lines
 * (pads set for the bindable slots), 0 if neither (nothing changed, so the caller keeps its defaults). */
int PesIniLoadBindings(const char *path, int keys[PES_PAD_SLOTS], char pads[PES_PAD_SLOTS][PES_PAD_NAME_MAX]);

#endif
