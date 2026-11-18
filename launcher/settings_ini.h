/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Keyboard bindings in the port's settings file (pes17recomp.ini, or PES_CONFIG): the key.<input>=
 * lines that the game reads at start (runtime_pad.c). Every other line of the file is kept as it is.
 * Plain C with stdio only, so it is tested on every platform. */
#ifndef PES_LAUNCHER_SETTINGS_INI_H
#define PES_LAUNCHER_SETTINGS_INI_H

#include "settings_keys.h"

/* The settings file the game and the settings window share: PES_CONFIG, else pes17recomp.ini. */
const char *PesConfigPath(void);

/* Writes the 24 key.<input>= lines (replacing any existing ones; an unbound slot is written empty).
 * Other lines, comments included, are kept in order. The file is replaced through a temporary file.
 * Returns 1 on success, 0 on failure (the file is then left as it was). */
int PesIniSaveKeyboard(const char *path, const int keys[PES_PAD_SLOTS]);

/* Reads the key.<input>= lines for the 24 slots into keys. Returns 1 if the file has any of them,
 * 0 if not (keys are then left unchanged, so the caller keeps its defaults). */
int PesIniLoadKeyboard(const char *path, int keys[PES_PAD_SLOTS]);

#endif
