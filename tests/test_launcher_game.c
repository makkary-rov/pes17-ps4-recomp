// SPDX-License-Identifier: GPL-2.0-or-later
// The launcher's game logic (launcher/launcher_game.c): a game folder needs eboot.bin; Play's
// arguments need the boot image and add the import aliases and content profile when present.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p, 0755)
#endif
#include "launcher_game.h"

static void Touch(const char *path) {
    FILE *f = fopen(path, "wb");
    assert(f);
    fputs("x", f);
    fclose(f);
}

int main(void) {
    const char *base = "test_launcher_base";
    const char *game = "test_launcher_game";
    MKDIR(base);
    MKDIR("test_launcher_base/verify");
    MKDIR("test_launcher_base/verify/m2b");
    MKDIR("test_launcher_base/verify/run1");
    MKDIR(game);
    remove("test_launcher_game/eboot.bin");
    remove("test_launcher_base/verify/m2b/boot-linked.bin");
    remove("test_launcher_base/verify/m2b/import-aliases.txt");
    remove("test_launcher_base/verify/run1/content.bin");

    /* A folder without eboot.bin is not the game; with it, it is. */
    assert(PesLauncherIsGameDir(game) == 0);
    assert(PesLauncherIsGameDir("") == 0 && PesLauncherIsGameDir(NULL) == 0);
    Touch("test_launcher_game/eboot.bin");
    assert(PesLauncherIsGameDir(game) == 1);

    char args[4096];
    /* No boot image: Play cannot start. */
    assert(PesLauncherPlayArgs(args, sizeof(args), base, game) == 0);

    /* The boot image alone: boot image and --app0, quoted. */
    Touch("test_launcher_base/verify/m2b/boot-linked.bin");
    assert(PesLauncherPlayArgs(args, sizeof(args), base, game) == 1);
    assert(strcmp(args, "\"test_launcher_base/verify/m2b/boot-linked.bin\" --app0 \"test_launcher_game\"") == 0);

    /* With the aliases and the content profile they are added. */
    Touch("test_launcher_base/verify/m2b/import-aliases.txt");
    Touch("test_launcher_base/verify/run1/content.bin");
    assert(PesLauncherPlayArgs(args, sizeof(args), base, game) == 1);
    assert(strstr(args, " --import-aliases \"test_launcher_base/verify/m2b/import-aliases.txt\"") != NULL);
    assert(strstr(args, " --content-profile \"test_launcher_base/verify/run1/content.bin\"") != NULL);

    /* Too small a buffer or no game folder: nothing. */
    assert(PesLauncherPlayArgs(args, 20, base, game) == 0);
    assert(PesLauncherPlayArgs(args, sizeof(args), base, "") == 0);

    remove("test_launcher_game/eboot.bin");
    remove("test_launcher_base/verify/m2b/boot-linked.bin");
    remove("test_launcher_base/verify/m2b/import-aliases.txt");
    remove("test_launcher_base/verify/run1/content.bin");
    printf("PASS: launcher game folder check and Play arguments\n");
    return 0;
}
