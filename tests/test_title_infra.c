/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Trace facility (T-1), patch engine (G-32) and read-only observer (G-44): synthetic data only. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "runtime_patch.h"
#include "title/cusa04957_patches.h"
#include "runtime_trace.h"

static void hex(const uint8_t d[32], char out[65]) {
    static const char digits[] = "0123456789abcdef";
    for (int i = 0; i < 32; i++) { out[i * 2] = digits[d[i] >> 4]; out[i * 2 + 1] = digits[d[i] & 15]; }
    out[64] = 0;
}

static void check_sha(const char *text, const char *expected) {
    uint8_t d[32];
    char h[65];
    pes_sha256((const uint8_t *)text, strlen(text), d);
    hex(d, h);
    if (strcmp(h, expected)) { fprintf(stderr, "sha256(\"%s\") = %s, expected %s\n", text, h, expected); abort(); }
}

static void check_repeat(size_t n, const char *expected) {
    char text[80];
    memset(text, 'a', n);
    text[n] = 0;
    check_sha(text, expected);
}

static void test_sha256(void) {
    check_sha("", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    check_sha("abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    /* padding boundaries: 55 bytes still fit one block, 56 and 63 need two, 64 is exactly one block + padding block */
    check_sha("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
              "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    check_repeat(55, "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318");
    check_repeat(63, "7d3e74a05d7db15bce4ad9ec0658ea98e3f06eeecf16b4c6fff2da457ddc2f34");
    check_repeat(64, "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb");
}

static void test_patch(void) {
    uint8_t image[64];
    for (int i = 0; i < 64; i++) image[i] = (uint8_t)(i + 1);
    uint8_t digest[32];
    char h[65];
    pes_sha256(image + 8, 4, digest);
    hex(digest, h);
    const uint8_t repl[4] = {0x90, 0x90, 0x90, 0x90};
    pes_patch ok = {"P-test-ok", 8, 4, h, repl};
    pes_patch_report r;
    assert(pes_patch_apply(image, sizeof(image), &ok, 1, &r) == 0);
    assert(r.applied == 1 && r.refused == 0);
    assert(!memcmp(image + 8, repl, 4));
    assert(image[7] == 8 && image[12] == 13);                 /* neighbours untouched */

    /* the same entry again: the original bytes are gone, so the signature no longer matches */
    assert(pes_patch_apply(image, sizeof(image), &ok, 1, &r) == 1);
    assert(r.applied == 0 && r.refused == 1);
    assert(!memcmp(image + 8, repl, 4));

    uint8_t before[64];
    memcpy(before, image, sizeof(image));
    pes_patch wrong = {"P-test-wrong-hash", 16, 4, "0000000000000000000000000000000000000000000000000000000000000000", repl};
    pes_patch range = {"P-test-range", 62, 4, h, repl};
    pes_patch overflow = {"P-test-overflow", UINT64_MAX - 1, 4, h, repl};
    pes_patch zero = {"P-test-zero", 0, 0, h, repl};
    pes_patch nohash = {"P-test-nohash", 0, 4, NULL, repl};
    pes_patch table[5] = {wrong, range, overflow, zero, nohash};
    assert(pes_patch_apply(image, sizeof(image), table, 5, &r) == 5);
    assert(r.applied == 0 && r.refused == 5);
    assert(!memcmp(image, before, sizeof(image)));            /* refused entries change nothing */
    assert(pes_patch_apply(image, sizeof(image), NULL, 0, &r) == 0);   /* empty table is a no-op */
}

static void test_observer(void) {
    uint8_t image[32] = {0};
    const pes_watch watches[3] = {{"mode", 4, 4}, {"flag", 9, 1}, {"outside", 30, 8}};
    pes_watch_state state[3];
    memset(state, 0, sizeof(state));
    assert(pes_observer_sample(image, sizeof(image), watches, 3, state) == 2);   /* first sample: 2 valid watches */
    assert(pes_observer_sample(image, sizeof(image), watches, 3, state) == 0);   /* unchanged */
    image[5] = 0x01;                                                              /* mode = 0x100 */
    assert(pes_observer_sample(image, sizeof(image), watches, 3, state) == 1);
    assert(state[0].last == 0x100);
    image[9] = 7;
    assert(pes_observer_sample(image, sizeof(image), watches, 3, state) == 1);
    assert(state[1].last == 7);
}

#ifndef PES_NO_TRACE
static void test_trace(void) {
    assert(pes_trace_parse(NULL) == 0);
    assert(pes_trace_parse("") == 0);
    assert(pes_trace_parse("loader") == PES_TRACE_LOADER);
    assert(pes_trace_parse("file,patch,bogus") == (PES_TRACE_FILE | PES_TRACE_PATCH));
    assert(pes_trace_parse("all") == (PES_TRACE_LOADER | PES_TRACE_FILE | PES_TRACE_GPU | PES_TRACE_TITLE |
                                      PES_TRACE_KERNEL | PES_TRACE_PATCH));
    assert(pes_trace_parse("title,,kernel") == (PES_TRACE_TITLE | PES_TRACE_KERNEL));

    /* output goes to a file, only enabled subsystems, with a cap */
    const char *path = "title-infra-trace.tmp";
    remove(path);
#ifdef _WIN32
    _putenv("PES_TRACE_FILE=title-infra-trace.tmp");
    _putenv("PES_TRACE_CAP=3");
#else
    setenv("PES_TRACE_FILE", path, 1);
    setenv("PES_TRACE_CAP", "3", 1);
#endif
    pes_trace_init("title");
    assert(pes_trace_enabled(PES_TRACE_TITLE) && !pes_trace_enabled(PES_TRACE_FILE));
    pes_trace(PES_TRACE_FILE, "ignored", "a=%d", 1);
    for (int i = 0; i < 10; i++) pes_trace(PES_TRACE_TITLE, "tick", "n=%d", i);
    pes_trace_shutdown();
    assert(!pes_trace_enabled(PES_TRACE_TITLE));
    FILE *f = fopen(path, "r");
    assert(f);
    char line[256];
    int lines = 0, caps = 0, ignored = 0;
    while (fgets(line, sizeof(line), f)) {
        lines++;
        if (strstr(line, "ev=cap_reached")) caps++;
        if (strstr(line, "ignored")) ignored++;
        if (lines == 1) assert(strstr(line, "sub=title ev=tick n=0"));
    }
    fclose(f);
    remove(path);
    assert(lines == 4 && caps == 1 && ignored == 0);          /* 3 events + 1 cap line */

    /* the observer and the patch engine log through the facility */
#ifdef _WIN32
    _putenv("PES_TRACE_CAP=100");
#else
    setenv("PES_TRACE_CAP", "100", 1);
#endif
    pes_trace_init("title,patch");
    uint8_t image[16] = {0};
    const pes_watch w = {"mode", 0, 4};
    pes_watch_state st = {0, 0};
    pes_observer_sample(image, sizeof(image), &w, 1, &st);
    pes_trace_shutdown();
    f = fopen(path, "r");
    assert(f && fgets(line, sizeof(line), f) && strstr(line, "ev=state name=mode value=0x0"));
    fclose(f);
    remove(path);
}
#endif

/* Cost of a disabled probe: the budget in the plan is 3% of frame time; here only a coarse bound. */
static void test_disabled_cost(void) {
    enum { N = 20000000 };
    volatile unsigned sink = 0;
#ifndef PES_NO_TRACE
    pes_trace_init(NULL);
#endif
    clock_t t0 = clock();
    for (unsigned i = 0; i < N; i++) { pes_trace(PES_TRACE_TITLE, "x", "i=%u", i); sink += i; }
    double ns = (double)(clock() - t0) * 1e9 / CLOCKS_PER_SEC / N;
    printf("disabled probe: %.2f ns per call\n", ns);
    assert(ns < 50.0);
    (void)sink;
}

int main(void) {
    test_sha256();
    test_patch();
    test_observer();
#ifndef PES_NO_TRACE
    test_trace();
#endif
    test_disabled_cost();
    printf("PASS: trace, patch engine and observer\n");
    return 0;
}
