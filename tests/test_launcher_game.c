// SPDX-License-Identifier: GPL-2.0-or-later
// The launcher's game logic (launcher/launcher_game.c): a game folder needs eboot.bin; the prepared
// files are all three; the preparation steps and Play's arguments are quoted correctly (spaces, a
// drive root, a non-ASCII folder), need the boot image, and add the aliases and content profile when
// present.
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
    const char *data = "test_launcher_data";
    const char *game = "test_launcher_game";
    MKDIR(data);
    MKDIR(game);
    remove("test_launcher_game/eboot.bin");
    remove("test_launcher_data/boot-linked.bin");
    remove("test_launcher_data/import-aliases.txt");
    remove("test_launcher_data/content.bin");
    remove("test_launcher_data/prepared.stamp");

    /* A folder without eboot.bin is not the game; with it, it is. */
    assert(PesLauncherIsGameDir(game) == 0);
    assert(PesLauncherIsGameDir("") == 0 && PesLauncherIsGameDir(NULL) == 0);
    Touch("test_launcher_game/eboot.bin");
    assert(PesLauncherIsGameDir(game) == 1);

    /* Prepared only when all three files exist AND the stamp says they are current for this game folder. */
    assert(PesLauncherPrepared(data, game) == 0);
    Touch("test_launcher_data/boot-linked.bin");
    Touch("test_launcher_data/import-aliases.txt");
    assert(PesLauncherPrepared(data, game) == 0);
    Touch("test_launcher_data/content.bin");
    assert(PesLauncherPrepared(data, game) == 0);                 /* files alone are not enough: no stamp */
    assert(PesLauncherWriteStamp(data, game) == 1);
    assert(PesLauncherPrepared(data, game) == 1);
    assert(PesLauncherPrepared(data, "another_game_folder") == 0); /* prepared for a different folder */
    assert(PesLauncherPrepared(data, "") == 0 && PesLauncherPrepared("", game) == 0);
    {   /* a stamp from another version of the preparation is not current */
        FILE *f = fopen("test_launcher_data/prepared.stamp", "wb");
        assert(f);
        fprintf(f, "v%d\n%s\n", PES_LAUNCHER_PREP_VERSION + 1, game);
        fclose(f);
        assert(PesLauncherPrepared(data, game) == 0);
    }
    assert(PesLauncherWriteStamp(data, game) == 1 && PesLauncherPrepared(data, game) == 1);
    assert(PesLauncherClearStamp(data) == 1 && PesLauncherPrepared(data, game) == 0); /* a preparation that starts clears it */
    assert(PesLauncherClearStamp(data) == 1);                                          /* clearing nothing is fine */

    /* The three steps: script under root/scripts, game folder, title config (not for step 1), output. */
    char args[4096];
    assert(PesLauncherStepArgs(0, args, sizeof(args), "C:/my repo", "C:/my data", "D:/games/CUSA04957") == 1);
    assert(strcmp(args, " \"C:/my repo/scripts/prepare.py\" \"D:/games/CUSA04957\" --title \"C:/my repo/src/title/CUSA04957.json\" --out \"C:/my data\"") == 0);
    assert(PesLauncherStepArgs(1, args, sizeof(args), "r", "d", "g") == 1);
    assert(strstr(args, "content_profile.py") != NULL && strstr(args, "--title") == NULL);
    assert(PesLauncherStepArgs(2, args, sizeof(args), "r", "d", "g") == 1 && strstr(args, "link_modules.py") != NULL);
    assert(PesLauncherStepArgs(3, args, sizeof(args), "r", "d", "g") == 0 && PesLauncherStepArgs(-1, args, sizeof(args), "r", "d", "g") == 0);
    assert(PesLauncherStepArgs(0, args, 20, "r", "d", "g") == 0);
    assert(PesLauncherStepName(0)[0] && PesLauncherStepName(PES_LAUNCHER_STEPS)[0] == '\0');

    /* Play: the boot image, --app0, aliases and content profile. */
    assert(PesLauncherPlayArgs(args, sizeof(args), data, game) == 1);
    assert(strcmp(args, " \"test_launcher_data/boot-linked.bin\" --app0 \"test_launcher_game\""
                        " --import-aliases \"test_launcher_data/import-aliases.txt\""
                        " --content-profile \"test_launcher_data/content.bin\"") == 0);
    remove("test_launcher_data/import-aliases.txt");
    remove("test_launcher_data/content.bin");
    assert(PesLauncherPlayArgs(args, sizeof(args), data, game) == 1 && strstr(args, "--import-aliases") == NULL);

    /* A game folder at a drive root: the trailing backslash is doubled, so the quote stays closed. */
    assert(PesLauncherPlayArgs(args, sizeof(args), data, "E:\\") == 1);
    assert(strstr(args, "--app0 \"E:\\\\\"") != NULL);
    /* A UTF-8 folder name passes through unchanged. */
    assert(PesLauncherPlayArgs(args, sizeof(args), data, "D:/J\xc3\xbcgos/CUSA04957") == 1);
    assert(strstr(args, "\"D:/J\xc3\xbcgos/CUSA04957\"") != NULL);

    /* No boot image, a too-small buffer, or no game folder: nothing. */
    remove("test_launcher_data/boot-linked.bin");
    assert(PesLauncherPlayArgs(args, sizeof(args), data, game) == 0);
    Touch("test_launcher_data/boot-linked.bin");
    assert(PesLauncherPlayArgs(args, 20, data, game) == 0);
    assert(PesLauncherPlayArgs(args, sizeof(args), data, "") == 0);

    remove("test_launcher_game/eboot.bin");
    remove("test_launcher_data/boot-linked.bin");
    remove("test_launcher_data/prepared.stamp");
    printf("PASS: launcher game folder, preparation steps and Play arguments\n");
    return 0;
}
