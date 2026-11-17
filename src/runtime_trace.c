/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Trace facility: see runtime_trace.h. */
#include "runtime_trace.h"

#ifndef PES_NO_TRACE
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

uint32_t pes_trace_mask = 0;

static FILE *out;
static int own_file;
static unsigned long cap = 20000;
static unsigned long counts[32];
static struct timespec start;

static const struct { const char *name; uint32_t bit; } subsystems[] = {
    {"loader", PES_TRACE_LOADER}, {"file", PES_TRACE_FILE}, {"gpu", PES_TRACE_GPU},
    {"title", PES_TRACE_TITLE}, {"kernel", PES_TRACE_KERNEL}, {"patch", PES_TRACE_PATCH}
};

uint32_t pes_trace_parse(const char *spec) {
    uint32_t mask = 0;
    char word[32];
    size_t n = 0;
    if (!spec) return 0;
    for (const char *p = spec;; p++) {
        if (*p == ',' || *p == 0) {
            word[n] = 0;
            if (!strcmp(word, "all")) mask = ~0u >> (32 - sizeof(subsystems) / sizeof(subsystems[0]));
            for (size_t i = 0; i < sizeof(subsystems) / sizeof(subsystems[0]); i++)
                if (!strcmp(word, subsystems[i].name)) mask |= subsystems[i].bit;
            n = 0;
            if (!*p) break;
        } else if (n + 1 < sizeof(word)) {
            word[n++] = *p;
        }
    }
    return mask;
}

void pes_trace_init(const char *spec) {
    pes_trace_shutdown();
    pes_trace_mask = pes_trace_parse(spec);
    memset(counts, 0, sizeof(counts));
    if (!pes_trace_mask) return;
    const char *capText = getenv("PES_TRACE_CAP");
    cap = 20000;
    if (capText && *capText) {
        unsigned long v = strtoul(capText, NULL, 10);
        if (v) cap = v;
    }
    const char *path = getenv("PES_TRACE_FILE");
    out = stderr;
    own_file = 0;
    if (path && *path) {
        FILE *f = fopen(path, "w");
        if (f) { out = f; own_file = 1; }
    }
    timespec_get(&start, TIME_UTC);
}

void pes_trace_shutdown(void) {
    if (own_file && out) fclose(out);
    out = NULL;
    own_file = 0;
    pes_trace_mask = 0;
}

void pes_trace_emit(uint32_t sub, const char *event, const char *fmt, ...) {
    size_t index = 0;
    const char *name = "?";
    if (!out) return;
    for (size_t i = 0; i < sizeof(subsystems) / sizeof(subsystems[0]); i++)
        if (subsystems[i].bit == sub) { index = i; name = subsystems[i].name; }
    if (++counts[index] > cap) {
        if (counts[index] == cap + 1) fprintf(out, "sub=%s ev=cap_reached cap=%lu\n", name, cap);
        return;
    }
    struct timespec now;
    timespec_get(&now, TIME_UTC);
    long long ms = (long long)(now.tv_sec - start.tv_sec) * 1000 + (now.tv_nsec - start.tv_nsec) / 1000000;
    fprintf(out, "t=%lld sub=%s ev=%s ", ms, name, event);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(out, fmt, ap);
    va_end(ap);
    fputc('\n', out);
}
#endif
