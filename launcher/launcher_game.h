/* SPDX-License-Identifier: GPL-2.0-or-later */
/* The launcher's game logic (no Windows calls except file opening, tested on every platform): whether a
 * folder holds the PS4 game (eboot.bin), the three one-time preparation steps that turn the game folder
 * into the files the loader needs, and the command line Play passes to the game. Paths are UTF-8. */
#ifndef PES_LAUNCHER_LAUNCHER_GAME_H
#define PES_LAUNCHER_LAUNCHER_GAME_H

#include <stddef.h>

/* 1 if dir holds the PS4 game (dir/eboot.bin exists), else 0. */
int PesLauncherIsGameDir(const char *dir);

/* 1 if data_dir holds the prepared files the loader needs (boot-linked.bin, import-aliases.txt and
 * content.bin), else 0. */
int PesLauncherPrepared(const char *data_dir);

/* The preparation steps, run once per game folder with Python 3 and no extra packages:
 * 0 prepare.py (the game's executable into boot.bin, analysis.json, import-aliases.txt),
 * 1 content_profile.py (content.bin), 2 link_modules.py (boot-linked.bin). */
#define PES_LAUNCHER_STEPS 3
const char *PesLauncherStepName(int step);

/* Writes the arguments of a step (everything after the Python program name): the script under
 * root/scripts, the game folder, and where the results go. The title config is root/src/title/
 * CUSA04957.json. Returns 1 on success, 0 for a bad step or a buffer that is too small. */
int PesLauncherStepArgs(int step, char *out, size_t cap, const char *root, const char *data_dir, const char *game_dir);

/* The game's arguments for Play: the boot image, --app0 <game_dir>, and the import aliases and the
 * content profile when present in data_dir. Paths are quoted (a trailing backslash is doubled so it
 * cannot escape the quote). Returns 1 on success, 0 if the boot image is missing or out is too small. */
int PesLauncherPlayArgs(char *out, size_t cap, const char *data_dir, const char *game_dir);

#endif
