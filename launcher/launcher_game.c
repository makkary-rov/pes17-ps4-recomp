/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "launcher_game.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

/* Opens a UTF-8 path (on Windows through the wide API, so any folder name works). */
static FILE *OpenUtf8(const char *path, const char *mode) {
#ifdef _WIN32
    wchar_t wide[2048], wmode[8];
    if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wide, 2048)) return NULL;
    if (!MultiByteToWideChar(CP_UTF8, 0, mode, -1, wmode, 8)) return NULL;
    return _wfopen(wide, wmode);
#else
    return fopen(path, mode);
#endif
}

static int Exists(const char *path) {
    FILE *f = OpenUtf8(path, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

static int JoinExists(const char *dir, const char *name) {
    char path[2048];
    if (snprintf(path, sizeof(path), "%s/%s", dir, name) >= (int)sizeof(path)) return 0;
    return Exists(path);
}

int PesLauncherIsGameDir(const char *dir) {
    return dir && dir[0] && JoinExists(dir, "eboot.bin");
}

/* The stamp text: the preparation version and the game folder the files were prepared for. */
static int StampText(char *out, size_t cap, const char *game_dir) {
    return snprintf(out, cap, "v%d\n%s\n", PES_LAUNCHER_PREP_VERSION, game_dir) < (int)cap;
}

int PesLauncherPrepared(const char *data_dir, const char *game_dir) {
    if (!data_dir || !data_dir[0] || !game_dir || !game_dir[0]) return 0;
    if (!JoinExists(data_dir, "boot-linked.bin") || !JoinExists(data_dir, "import-aliases.txt") ||
        !JoinExists(data_dir, "content.bin")) return 0;
    char path[2048], expected[2048], found[2048];
    if (snprintf(path, sizeof(path), "%s/prepared.stamp", data_dir) >= (int)sizeof(path)) return 0;
    if (!StampText(expected, sizeof(expected), game_dir)) return 0;
    FILE *f = OpenUtf8(path, "rb");
    if (!f) return 0;
    const size_t n = fread(found, 1, sizeof(found) - 1, f);
    fclose(f);
    found[n] = 0;
    return strcmp(found, expected) == 0;
}

int PesLauncherClearStamp(const char *data_dir) {
    char path[2048];
    if (!data_dir || snprintf(path, sizeof(path), "%s/prepared.stamp", data_dir) >= (int)sizeof(path)) return 0;
#ifdef _WIN32
    wchar_t wide[2048];
    if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wide, 2048)) return 0;
    return DeleteFileW(wide) != 0 || GetLastError() == ERROR_FILE_NOT_FOUND;
#else
    return remove(path) == 0 || !Exists(path);
#endif
}

int PesLauncherWriteStamp(const char *data_dir, const char *game_dir) {
    char path[2048], text[2048];
    if (!data_dir || !game_dir || snprintf(path, sizeof(path), "%s/prepared.stamp", data_dir) >= (int)sizeof(path)) return 0;
    if (!StampText(text, sizeof(text), game_dir)) return 0;
    FILE *f = OpenUtf8(path, "wb");
    if (!f) return 0;
    const int ok = fputs(text, f) >= 0;
    return fclose(f) == 0 && ok;
}

const char *PesLauncherStepName(int step) {
    static const char *kNames[PES_LAUNCHER_STEPS] = {"Reading the game's executable", "Reading the game's content list",
                                                     "Linking the game's modules"};
    return step >= 0 && step < PES_LAUNCHER_STEPS ? kNames[step] : "";
}

/* Appends text; returns 0 when it does not fit. */
static int Append(char *out, size_t cap, size_t *len, const char *text) {
    const size_t n = strlen(text);
    if (*len + n + 1 > cap) return 0;
    memcpy(out + *len, text, n + 1);
    *len += n;
    return 1;
}

/* Appends a space, then path in quotes. A run of backslashes at the end is doubled, because a
 * backslash before the closing quote would escape it (a game folder at a drive root: E:\). */
static int AppendQuoted(char *out, size_t cap, size_t *len, const char *path) {
    if (!Append(out, cap, len, " \"") || !Append(out, cap, len, path)) return 0;
    size_t trailing = 0;
    for (size_t i = strlen(path); i > 0 && path[i - 1] == '\\'; --i) ++trailing;
    for (size_t i = 0; i < trailing; ++i) {
        if (!Append(out, cap, len, "\\")) return 0;
    }
    return Append(out, cap, len, "\"");
}

int PesLauncherStepArgs(int step, char *out, size_t cap, const char *root, const char *data_dir, const char *game_dir) {
    if (!out || cap == 0 || !root || !data_dir || !game_dir || !game_dir[0]) return 0;
    static const char *kScripts[PES_LAUNCHER_STEPS] = {"prepare.py", "content_profile.py", "link_modules.py"};
    if (step < 0 || step >= PES_LAUNCHER_STEPS) return 0;
    out[0] = '\0';
    size_t len = 0;
    char path[2048];
    snprintf(path, sizeof(path), "%s/scripts/%s", root, kScripts[step]);
    int ok = AppendQuoted(out, cap, &len, path);
    ok = ok && AppendQuoted(out, cap, &len, game_dir);
    if (step != 1) {  /* content_profile.py takes no title config */
        snprintf(path, sizeof(path), "%s/src/title/CUSA04957.json", root);
        ok = ok && Append(out, cap, &len, " --title") && AppendQuoted(out, cap, &len, path);
    }
    ok = ok && Append(out, cap, &len, " --out") && AppendQuoted(out, cap, &len, data_dir);
    if (!ok) out[0] = '\0';
    return ok;
}

int PesLauncherPlayArgs(char *out, size_t cap, const char *data_dir, const char *game_dir) {
    if (!out || cap == 0 || !data_dir || !game_dir || !game_dir[0]) return 0;
    out[0] = '\0';
    char path[2048];
    if (!JoinExists(data_dir, "boot-linked.bin")) return 0;
    size_t len = 0;
    snprintf(path, sizeof(path), "%s/boot-linked.bin", data_dir);
    int ok = AppendQuoted(out, cap, &len, path) && Append(out, cap, &len, " --app0") && AppendQuoted(out, cap, &len, game_dir);
    if (ok && JoinExists(data_dir, "import-aliases.txt")) {
        snprintf(path, sizeof(path), "%s/import-aliases.txt", data_dir);
        ok = Append(out, cap, &len, " --import-aliases") && AppendQuoted(out, cap, &len, path);
    }
    if (ok && JoinExists(data_dir, "content.bin")) {
        snprintf(path, sizeof(path), "%s/content.bin", data_dir);
        ok = Append(out, cap, &len, " --content-profile") && AppendQuoted(out, cap, &len, path);
    }
    if (!ok) out[0] = '\0';
    return ok;
}
