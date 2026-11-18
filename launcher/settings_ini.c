/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "settings_ini.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *PesConfigPath(void) {
    const char *env = getenv("PES_CONFIG");
    return env && env[0] ? env : "pes17recomp.ini";
}

/* The input name of a key.<input>= line, or NULL if the line is not one of ours. */
static int SlotOfLine(const char *line) {
    if (strncmp(line, "key.", 4) != 0) return -1;
    const char *eq = strchr(line, '=');
    if (!eq) return -1;
    const size_t n = (size_t)(eq - (line + 4));
    for (int s = 0; s < PES_PAD_SLOTS; ++s) {
        const char *name = PesSlotInputName(s);
        if (strlen(name) == n && strncmp(line + 4, name, n) == 0) return s;
    }
    return -1;
}

/* Reads the whole file into a buffer. Returns NULL if the file does not exist. */
static char *ReadAll(const char *path, size_t *size_out) {
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
    *size_out = got;
    return buf;
}

int PesIniSaveKeyboard(const char *path, const int keys[PES_PAD_SLOTS]) {
    size_t size = 0;
    char *old = ReadAll(path, &size);
    if (!old && fopen(path, "rb")) return 0; /* exists but unreadable */
    if (!old) {
        old = malloc(1);
        if (!old) return 0;
        old[0] = '\0';
        size = 0;
    }
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *out = fopen(tmp, "wb");
    if (!out) { free(old); return 0; }
    /* Keep every line that is not one of our key.<input>= lines, in order. */
    const char *p = old;
    while (*p) {
        const char *end = strchr(p, '\n');
        const size_t len = end ? (size_t)(end - p) + 1 : strlen(p);
        char line[1024];
        const size_t copy = len < sizeof(line) - 1 ? len : sizeof(line) - 1;
        memcpy(line, p, copy);
        line[copy] = '\0';
        if (SlotOfLine(line) < 0) {
            if (fwrite(p, 1, len, out) != len) { fclose(out); remove(tmp); free(old); return 0; }
            if (len > 0 && p[len - 1] != '\n') fputc('\n', out);
        }
        p += len;
    }
    (void)size;
    fprintf(out, "# keyboard bindings (settings screen)\n");
    for (int s = 0; s < PES_PAD_SLOTS; ++s) {
        char name[32];
        if (keys[s] == 0 || !PesKeySdlName(keys[s], name, sizeof(name))) name[0] = '\0';
        fprintf(out, "key.%s=%s\n", PesSlotInputName(s), name);
    }
    const int ok = fclose(out) == 0;
    free(old);
    if (!ok) { remove(tmp); return 0; }
    remove(path);
    if (rename(tmp, path) != 0) { remove(tmp); return 0; }
    return 1;
}

int PesIniLoadKeyboard(const char *path, int keys[PES_PAD_SLOTS]) {
    size_t size = 0;
    char *text = ReadAll(path, &size);
    if (!text) return 0;
    int found[PES_PAD_SLOTS] = {0};
    int loaded[PES_PAD_SLOTS];
    for (int s = 0; s < PES_PAD_SLOTS; ++s) loaded[s] = 0;
    const char *p = text;
    while (*p) {
        const char *end = strchr(p, '\n');
        char line[1024];
        const size_t len = end ? (size_t)(end - p) : strlen(p);
        const size_t copy = len < sizeof(line) - 1 ? len : sizeof(line) - 1;
        memcpy(line, p, copy);
        line[copy] = '\0';
        const int s = SlotOfLine(line);
        if (s >= 0) {
            found[s] = 1;
            /* First name of the value, trimmed of spaces and a trailing \r. */
            char *value = strchr(line, '=') + 1;
            while (*value == ' ' || *value == '\t') ++value;
            char *comma = strchr(value, ',');
            if (comma) *comma = '\0';
            size_t vlen = strlen(value);
            while (vlen > 0 && (value[vlen - 1] == ' ' || value[vlen - 1] == '\r' || value[vlen - 1] == '\t')) value[--vlen] = '\0';
            loaded[s] = vlen ? PesKeyFromSdlName(value) : 0;
        }
        if (!end) break;
        p = end + 1;
    }
    free(text);
    int any = 0;
    for (int s = 0; s < PES_PAD_SLOTS; ++s) any = any || found[s];
    if (!any) return 0;
    for (int s = 0; s < PES_PAD_SLOTS; ++s) keys[s] = loaded[s];
    return 1;
}
