/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "settings_ini.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

/* PES_CONFIG, else pes17recomp.ini next to this executable (the game uses the same rule, so both
 * read the same file whatever the working directory). Off Windows: the working directory. */
const char *PesConfigPath(void) {
    const char *env = getenv("PES_CONFIG");
    if (env && env[0]) return env;
#ifdef _WIN32
    static char path[1024];
    if (!path[0]) {
        const DWORD n = GetModuleFileNameA(NULL, path, sizeof(path));
        char *cut = n > 0 && n < sizeof(path) ? strrchr(path, '\\') : NULL;
        if (cut && (size_t)(cut - path) + 1 + sizeof("pes17recomp.ini") <= sizeof(path)) strcpy(cut + 1, "pes17recomp.ini");
        else strcpy(path, "pes17recomp.ini");
    }
    return path;
#else
    return "pes17recomp.ini";
#endif
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

/* The file's text for a save: empty for a new file, NULL if it exists but cannot be read. */
static char *OldText(const char *path) {
    char *old = ReadAll(path);
    if (old) return old;
    FILE *probe = fopen(path, "rb");
    if (probe) { fclose(probe); return NULL; }
    old = malloc(1);
    if (old) old[0] = '\0';
    return old;
}

/* Copies the lines of old to out, in order, leaving out the binding lines (drop_bindings) and/or the
 * pad_input line (drop_method). Returns 0 on a write error. */
static int CopyKept(const char *old, FILE *out, int drop_method, int drop_bindings) {
    const char *p = old;
    while (*p) {
        const char *end = strchr(p, '\n');
        const size_t len = end ? (size_t)(end - p) + 1 : strlen(p);
        char line[1024];
        const size_t copy = len < sizeof(line) - 1 ? len : sizeof(line) - 1;
        memcpy(line, p, copy);
        line[copy] = '\0';
        int drop = 0;
        if (drop_bindings && SlotOfLine(line, NULL) >= 0) drop = 1;
        if (drop_method && strncmp(p, "pad_input=", 10) == 0) drop = 1;
        if (!drop) {
            if (fwrite(p, 1, len, out) != len) return 0;
            if (len > 0 && p[len - 1] != '\n') fputc('\n', out);
        }
        p += len;
    }
    return 1;
}

/* Replaces the file with the temporary one in one step, so a crash or a failure never leaves the settings
 * missing: the old file stays until the new one is in place. Returns 1 on success. */
static int SwapInFile(const char *path, const char *tmp) {
#ifdef _WIN32
    const int ok = MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    const int ok = rename(tmp, path) == 0; /* POSIX rename replaces atomically */
#endif
    if (!ok) remove(tmp);
    return ok;
}

int PesIniSaveBindings(const char *path, const int keys[PES_PAD_SLOTS], const char pads[PES_PAD_SLOTS][PES_PAD_NAME_MAX]) {
    char *old = OldText(path);
    if (!old) return 0;
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *out = fopen(tmp, "wb");
    if (!out) { free(old); return 0; }
    /* Keep every line that is not one of our key./pad. lines, in order. */
    const int kept = CopyKept(old, out, 0, 1);
    free(old);
    if (!kept) { fclose(out); remove(tmp); return 0; }
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
    if (!ok) { remove(tmp); return 0; }
    return SwapInFile(path, tmp);
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

int PesIniSavePadMethod(const char *path, int method) {
    char *old = OldText(path);
    if (!old) return 0;
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *out = fopen(tmp, "wb");
    if (!out) { free(old); return 0; }
    const int kept = CopyKept(old, out, 1, 0);
    free(old);
    if (!kept) { fclose(out); remove(tmp); return 0; }
    fprintf(out, "pad_input=%s\n", method == PES_PAD_DIRECTINPUT ? "directinput" : "xinput");
    const int ok = fclose(out) == 0;
    if (!ok) { remove(tmp); return 0; }
    return SwapInFile(path, tmp);
}

int PesIniLoadPadMethod(const char *path, int *method) {
    char *text = ReadAll(path);
    if (!text) return 0;
    int found = 0;
    const char *p = text;
    while (*p) {
        const char *end = strchr(p, '\n');
        if (strncmp(p, "pad_input=", 10) == 0) {
            found = 1;
            *method = strncmp(p + 10, "directinput", 11) == 0 ? PES_PAD_DIRECTINPUT : PES_PAD_XINPUT;
        }
        if (!end) break;
        p = end + 1;
    }
    free(text);
    return found;
}

/* The text after key= (trailing spaces and CR removed). Returns 1 if the file has the key; the last line
 * with it wins, as the game reads it. */
int PesIniLoadValue(const char *path, const char *key, char *buf, size_t size) {
    if (!buf || size == 0) return 0;
    buf[0] = '\0';
    char *text = ReadAll(path);
    if (!text) return 0;
    char prefix[64];
    snprintf(prefix, sizeof(prefix), "%s=", key);
    const size_t plen = strlen(prefix);
    int found = 0;
    const char *p = text;
    while (*p) {
        const char *end = strchr(p, '\n');
        if (strncmp(p, prefix, plen) == 0) {
            const char *start = p + plen;
            size_t len = end ? (size_t)(end - start) : strlen(start);
            while (len > 0 && (start[len - 1] == ' ' || start[len - 1] == '\r' || start[len - 1] == '\t')) --len;
            if (len >= size) len = size - 1;
            memcpy(buf, start, len);
            buf[len] = '\0';
            found = 1;
        }
        if (!end) break;
        p = end + 1;
    }
    free(text);
    return found;
}

/* Writes several key=value settings in one pass. Each key's first line is replaced in place (a repeated
 * line of the same key is dropped); keys not in the file are appended. Other lines are kept. Up to 32 keys. */
int PesIniSaveValues(const char *path, const char *const keys[], const char *const values[], int count) {
    if (count <= 0 || count > 32) return 0;
    char *old = OldText(path);
    if (!old) return 0;
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *out = fopen(tmp, "wb");
    if (!out) { free(old); return 0; }
    int written[32] = {0};
    int bad = 0;
    const char *p = old;
    while (*p && !bad) {
        const char *end = strchr(p, '\n');
        const size_t len = end ? (size_t)(end - p) + 1 : strlen(p);
        int match = -1;
        for (int k = 0; k < count && match < 0; ++k) {
            const size_t kl = strlen(keys[k]);
            if (strncmp(p, keys[k], kl) == 0 && p[kl] == '=') match = k;
        }
        if (match < 0) {
            if (fwrite(p, 1, len, out) != len) bad = 1;
            else if (p[len - 1] != '\n') fputc('\n', out);
        } else if (!written[match]) {
            fprintf(out, "%s=%s\n", keys[match], values[match]);
            written[match] = 1;
        }
        p += len;
    }
    free(old);
    for (int k = 0; k < count && !bad; ++k) {
        if (!written[k]) fprintf(out, "%s=%s\n", keys[k], values[k]);
    }
    const int closed = fclose(out) == 0;
    if (bad || !closed) { remove(tmp); return 0; }
    return SwapInFile(path, tmp);
}

/* Writes one whole-number setting, key=value, replacing its line (or appending it). Other lines are kept. */
int PesIniSaveInt(const char *path, const char *key, int value) {
    char *old = OldText(path);
    if (!old) return 0;
    char prefix[64];
    snprintf(prefix, sizeof(prefix), "%s=", key);
    const size_t plen = strlen(prefix);
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *out = fopen(tmp, "wb");
    if (!out) { free(old); return 0; }
    const char *p = old;
    while (*p) {
        const char *end = strchr(p, '\n');
        const size_t len = end ? (size_t)(end - p) + 1 : strlen(p);
        if (strncmp(p, prefix, plen) != 0) {
            if (fwrite(p, 1, len, out) != len) { free(old); fclose(out); remove(tmp); return 0; }
            if (p[len - 1] != '\n') fputc('\n', out);
        }
        p += len;
    }
    free(old);
    fprintf(out, "%s%d\n", prefix, value);
    if (fclose(out) != 0) { remove(tmp); return 0; }
    return SwapInFile(path, tmp);
}

/* Reads one whole-number setting. Returns 1 if the file has a number for the key, else 0 (value unchanged). */
int PesIniLoadInt(const char *path, const char *key, int *value) {
    char *text = ReadAll(path);
    if (!text) return 0;
    char prefix[64];
    snprintf(prefix, sizeof(prefix), "%s=", key);
    const size_t plen = strlen(prefix);
    int found = 0;
    const char *p = text;
    while (*p) {
        const char *end = strchr(p, '\n');
        if (strncmp(p, prefix, plen) == 0) {
            char *stop = NULL;
            const long v = strtol(p + plen, &stop, 10);
            if (stop != p + plen) {
                *value = (int)v;
                found = 1;
            }
        }
        if (!end) break;
        p = end + 1;
    }
    free(text);
    return found;
}
