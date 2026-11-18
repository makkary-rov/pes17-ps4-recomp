// SPDX-License-Identifier: GPL-2.0-or-later
// A crash keeps a minidump for later review (src/win32_crash.c): the writer produces a file
// that starts with the minidump signature "MDMP".
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "crash_win.h"

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "minidump-test.dmp";
    remove(path);
    if (!crash_win_write_minidump(path, NULL)) {
        printf("FAIL: no minidump written to %s\n", path);
        return EXIT_FAILURE;
    }
    char sig[4] = {0};
    FILE *f = fopen(path, "rb");
    size_t n = f ? fread(sig, 1, 4, f) : 0;
    if (f) fclose(f);
    remove(path);
    if (n != 4 || memcmp(sig, "MDMP", 4) != 0) {
        printf("FAIL: %s is not a minidump\n", path);
        return EXIT_FAILURE;
    }
    printf("PASS: crash minidump written and readable\n");
    return EXIT_SUCCESS;
}
