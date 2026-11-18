/* SPDX-License-Identifier: GPL-2.0-or-later */
/* The Display tab in the settings file: the keys the game reads (gpu/shim/pes_settings.cpp), written
 * from and read into the Display model. Plain C with stdio, tested on every platform. */
#ifndef PES_LAUNCHER_SETTINGS_DISPLAY_INI_H
#define PES_LAUNCHER_SETTINGS_DISPLAY_INI_H

#include "settings_model.h"

/* Writes the Display tab's settings to the file, keeping every other line. Two values the window cannot
 * show are kept when they are already in the file: an upscaler other than FSR 3.1 (FSR 4, FSR 4.1.1,
 * TAA, DLSS, chosen in the in-game menu) while FSR is on, and an output size outside the fixed list
 * while the list shows its fallback. Returns 1 on success, 0 on failure. */
int PesIniSaveDisplay(const char *path, PesDisplaySettings s);

/* Reads the Display tab's settings from the file into s. Keys the file does not have keep the values
 * already in s (the caller starts from PesDisplayDefaults). Returns 1 if the file could be read. */
int PesIniLoadDisplay(const char *path, PesDisplaySettings *s);

#endif
