/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "settings_ini.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *PesConfigPath(void) {
    const char *env = getenv("PES_CONFIG");
    return env && env[0] ? env : "pes17recomp.ini";
}

/* Which slot a line belongs to: key.<input>= (any slot) or pad.<input>= (bindable slots only).
 * Returns the slot, or -1 for other lines; *is_pad is set for pad lines. */
static int SlotOfLine(const char *line, int *is_pad) {
    const char *eq = strchr(line, '=');
    if (!eq) return -1;
    int pad;
    if (strncmp(line, "key.", 4) == 0) pad = 0;
    else if (strncmp(line, "pad.", 4) == 0) pad = 1;
    else return -1;
    const size_t n = (size_t)(eq - (line + 4));
    for (int s = 0; s < PES_PAD_SLOTS; ++s) {
        if (pad && !PesPadBindable(s)) continue;
        const char *name = PesSlotInputName(s);
        if (strlen(name) == n && strncmp(line + 4, name, n) == 0) {
            if (is_pad) *is_pad = pad;
            return s;
        }
    }
    return -1;
}

/* Reads the whole file into a buffer. Returns NULL if it cannot be read. */
static char *ReadAll(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) { fclose(f); return NULL; }
    char *buf = malloc((size_t)size + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[got] = '\0';
    return buf;
}

int PesIniSaveBindings(const char *path, const int keys[PES_PAD_SLOTS], const char pads[PES_PAD_SLOTS][PES_PAD_NAME_MAX]) {
    char *old = ReadAll(path);
    if (!old) {
        /* A missing file is fine; an unreadable one is not. */
        FILE *probe = fopen(path, "rb");
        if (probe) { fclose(probe); return 0; }
        old = malloc(1);
        if (!old) return 0;
        old[0] = '\0';
    }
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *out = fopen(tmp, "wb");
    if (!out) { free(old); return 0; }
    /* Keep every line that is not one of our key./pad. lines, in order. */
    const char *p = old;
    while (*p) {
        const char *end = strchr(p, '\n');
        const size_t len = end ? (size_t)(end - p) + 1 : strlen(p);
        char line[1024];
        const size_t copy = len < sizeof(line) - 1 ? len : sizeof(line) - 1;
        memcpy(line, p, copy);
        line[copy] = '\0';
        if (SlotOfLine(line, NULL) < 0) {
            if (fwrite(p, 1, len, out) != len) { fclose(out); remove(tmp); free(old); return 0; }
            if (len > 0 && p[len - 1] != '\n') fputc('\n', out);
        }
        p += len;
    }
    fprintf(out, "# bindings (settings screen)\n");
    for (int s = 0; s < PES_PAD_SLOTS; ++s) {
        char name[32];
        if (keys[s] == 0 || !PesKeySdlName(keys[s], name, sizeof(name))) name[0] = '\0';
        fprintf(out, "key.%s=%s\n", PesSlotInputName(s), name);
    }
    for (int s = 0; s < PES_PAD_SLOTS; ++s) {
        if (!PesPadBindable(s)) continue;
        fprintf(out, "pad.%s=%s\n", PesSlotInputName(s), pads[s]);
    }
    const int ok = fclose(out) == 0;
    free(old);
    if (!ok) { remove(tmp); return 0; }
    remove(path);
    if (rename(tmp, path) != 0) { remove(tmp); return 0; }
    return 1;
}

int PesIniLoadBindings(const char *path, int keys[PES_PAD_SLOTS], char pads[PES_PAD_SLOTS][PES_PAD_NAME_MAX]) {
    char *text = ReadAll(path);
    if (!text) return 0;
    int key_found = 0, pad_found = 0;
    int loaded_keys[PES_PAD_SLOTS];
    char loaded_pads[PES_PAD_SLOTS][PES_PAD_NAME_MAX];
    for (int s = 0; s < PES_PAD_SLOTS; ++s) {
        loaded_keys[s] = 0;
        loaded_pads[s][0] = '\0';
    }
    const char *p = text;
    while (*p) {
        const char *end = strchr(p, '\n');
        char line[1024];
        const size_t len = end ? (size_t)(end - p) : strlen(p);
        const size_t copy = len < sizeof(line) - 1 ? len : sizeof(line) - 1;
        memcpy(line, p, copy);
        line[copy] = '\0';
        int is_pad = 0;
        const int s = SlotOfLine(line, &is_pad);
        if (s >= 0) {
            /* The value's first name, without spaces or a trailing \r. */
            char *value = strchr(line, '=') + 1;
            while (*value == ' ' || *value == '\t') ++value;
            char *comma = strchr(value, ',');
            if (comma) *comma = '\0';
            size_t vlen = strlen(value);
            while (vlen > 0 && (value[vlen - 1] == ' ' || value[vlen - 1] == '\r' || value[vlen - 1] == '\t')) value[--vlen] = '\0';
            if (is_pad) {
                pad_found = 1;
                snprintf(loaded_pads[s], PES_PAD_NAME_MAX, "%s", value);
            } else {
                key_found = 1;
                loaded_keys[s] = vlen ? PesKeyFromSdlName(value) : 0;
            }
        }
        if (!end) break;
        p = end + 1;
    }
    free(text);
    if (key_found) for (int s = 0; s < PES_PAD_SLOTS; ++s) keys[s] = loaded_keys[s];
    if (pad_found) {
        for (int s = 0; s < PES_PAD_SLOTS; ++s) {
            if (PesPadBindable(s)) snprintf(pads[s], PES_PAD_NAME_MAX, "%s", loaded_pads[s]);
        }
    }
    return (key_found ? 1 : 0) | (pad_found ? 2 : 0);
}
