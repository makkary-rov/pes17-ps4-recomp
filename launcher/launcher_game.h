/* SPDX-License-Identifier: GPL-2.0-or-later */
/* The launcher's game logic (no Windows calls, tested on every platform): whether a folder holds the
 * PS4 game (eboot.bin), and the command line Play passes to the game. */
#ifndef PES_LAUNCHER_LAUNCHER_GAME_H
#define PES_LAUNCHER_LAUNCHER_GAME_H

#include <stddef.h>

/* 1 if dir holds the PS4 game (dir/eboot.bin exists), else 0. */
int PesLauncherIsGameDir(const char *dir);

/* The game's arguments for Play: the boot image, --app0 <game_dir>, and the import aliases and the
 * content profile when present. They are looked for under base (the launcher's folder):
 * verify/m2b/boot-linked.bin, verify/m2b/import-aliases.txt, verify/run1/content.bin. Paths are
 * quoted. Returns 1 on success, 0 if the boot image is missing or out is too small. */
int PesLauncherPlayArgs(char *out, size_t cap, const char *base, const char *game_dir);

#endif
