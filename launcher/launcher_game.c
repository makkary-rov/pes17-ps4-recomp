/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "launcher_game.h"
#include <stdio.h>
#include <string.h>

static int Exists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

int PesLauncherIsGameDir(const char *dir) {
    if (!dir || !dir[0]) return 0;
    char path[1024];
    if (snprintf(path, sizeof(path), "%s/eboot.bin", dir) >= (int)sizeof(path)) return 0;
    return Exists(path);
}

/* Appends text; returns 0 when it does not fit. */
static int Append(char *out, size_t cap, size_t *len, const char *text) {
    const size_t n = strlen(text);
    if (*len + n + 1 > cap) return 0;
    memcpy(out + *len, text, n + 1);
    *len += n;
    return 1;
}

int PesLauncherPlayArgs(char *out, size_t cap, const char *base, const char *game_dir) {
    if (!out || cap == 0 || !base || !game_dir || !game_dir[0]) return 0;
    out[0] = '\0';
    char boot[1024], aliases[1024], content[1024], part[1200];
    snprintf(boot, sizeof(boot), "%s/verify/m2b/boot-linked.bin", base);
    snprintf(aliases, sizeof(aliases), "%s/verify/m2b/import-aliases.txt", base);
    snprintf(content, sizeof(content), "%s/verify/run1/content.bin", base);
    if (!Exists(boot)) return 0;
    size_t len = 0;
    snprintf(part, sizeof(part), "\"%s\" --app0 \"%s\"", boot, game_dir);
    if (!Append(out, cap, &len, part)) return 0;
    if (Exists(aliases)) {
        snprintf(part, sizeof(part), " --import-aliases \"%s\"", aliases);
        if (!Append(out, cap, &len, part)) return 0;
    }
    if (Exists(content)) {
        snprintf(part, sizeof(part), " --content-profile \"%s\"", content);
        if (!Append(out, cap, &len, part)) return 0;
    }
    return 1;
}
