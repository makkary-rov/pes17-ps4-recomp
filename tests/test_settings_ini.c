// SPDX-License-Identifier: GPL-2.0-or-later
// Keyboard bindings in the settings file (launcher/settings_ini.c): the key.<input>= lines are written
// and read back, every other line (comments, other keys) is kept, and an unbound slot stays unbound.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "settings_ini.h"

static char *ReadFile(const char *path) {
    FILE *f = fopen(path, "rb");
    assert(f);
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    assert(buf);
    assert(fread(buf, 1, (size_t)n, f) == (size_t)n);
    fclose(f);
    buf[n] = '\0';
    return buf;
}

static void WriteFile(const char *path, const char *text) {
    FILE *f = fopen(path, "wb");
    assert(f);
    fputs(text, f);
    fclose(f);
}

int main(void) {
    const char *path = "test_settings_ini.ini";
    assert(strcmp(PesSlotInputName(PES_SLOT_X), "square") == 0);
    assert(strcmp(PesSlotInputName(PES_SLOT_RS_RIGHT), "look_right") == 0);

    /* A settings file from the port: other keys, comments, an old key line and a line of an input
     * the screen does not own (touchpad_right). */
    WriteFile(path,
              "# launcher\n"
              "upscaler=fsr3\r\n"
              "key.cross=Space\n"
              "key.touchpad_right=Backspace\n"
              "pad.circle=a\n");

    int keys[PES_PAD_SLOTS];
    PesKeyboardDefaults(keys);
    assert(PesIniSaveKeyboard(path, keys) == 1);
    char *text = ReadFile(path);
    assert(strstr(text, "# launcher\n") != NULL);
    assert(strstr(text, "upscaler=fsr3") != NULL);
    assert(strstr(text, "pad.circle=a\n") != NULL);
    assert(strstr(text, "key.touchpad_right=Backspace\n") != NULL);
    assert(strstr(text, "key.cross=Space") == NULL);          /* replaced */
    assert(strstr(text, "key.cross=X\n") != NULL);            /* slot A = cross, default X */
    assert(strstr(text, "key.square=A\n") != NULL);           /* slot X = square, default A */
    assert(strstr(text, "key.up=Keypad 8\n") != NULL);        /* d-pad Up, numpad 8 */
    assert(strstr(text, "key.l3=\n") != NULL);                /* LS press: unbound, written empty */
    free(text);

    /* Read back: the same bindings. */
    int back[PES_PAD_SLOTS];
    for (int s = 0; s < PES_PAD_SLOTS; ++s) back[s] = -1;
    assert(PesIniLoadKeyboard(path, back) == 1);
    for (int s = 0; s < PES_PAD_SLOTS; ++s) assert(back[s] == keys[s]);

    /* Unbinding and rebinding survive a save and a load. */
    assert(PesKeyboardBind(keys, PES_SLOT_RS_UP, 'R') == 1);
    assert(PesKeyboardBind(keys, PES_SLOT_LS_PRESS, 0) == 1);
    assert(PesIniSaveKeyboard(path, keys) == 1);
    assert(PesIniLoadKeyboard(path, back) == 1);
    assert(back[PES_SLOT_RS_UP] == 'R' && back[PES_SLOT_LS_PRESS] == 0);
    text = ReadFile(path);
    assert(strstr(text, "key.look_up=R\n") != NULL);
    /* The other lines are still there, once each. */
    assert(strstr(text, "key.touchpad_right=Backspace\n") != NULL);
    assert(strstr(text, "# launcher\n") == text);
    free(text);

    /* A file with none of our lines loads nothing and leaves the caller's values alone. */
    WriteFile(path, "upscaler=fsr3\npad.circle=a\n");
    int kept[PES_PAD_SLOTS];
    PesKeyboardDefaults(kept);
    assert(PesIniLoadKeyboard(path, kept) == 0);
    assert(kept[PES_SLOT_X] == 'A' && kept[PES_SLOT_RS_UP] == 0);

    /* A missing file loads nothing; saving creates it. */
    remove(path);
    assert(PesIniLoadKeyboard(path, kept) == 0);
    PesKeyboardDefaults(kept);
    assert(PesIniSaveKeyboard(path, kept) == 1);
    assert(PesIniLoadKeyboard(path, back) == 1);
    for (int s = 0; s < PES_PAD_SLOTS; ++s) assert(back[s] == kept[s]);

    remove(path);
    printf("PASS: keyboard bindings saved and loaded, other settings kept\n");
    return 0;
}
