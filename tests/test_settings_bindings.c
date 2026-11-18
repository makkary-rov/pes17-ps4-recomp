// SPDX-License-Identifier: GPL-2.0-or-later
// Bindings in the settings file (launcher/settings_ini.c): key. and pad. lines are written and read
// back, every other line (comments, other keys, lines of inputs the screen does not own) is kept, the
// analog stick directions get no pad line, and an unbound slot stays unbound. The pad_input line
// (XInput or DirectInput) is written, replaced and read back as well.
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
    const char *path = "test_settings_bindings.ini";
    char pads[PES_PAD_SLOTS][PES_PAD_NAME_MAX];
    int keys[PES_PAD_SLOTS];
    PesKeyboardDefaults(keys);
    PesPadDefaults(pads);

    /* A file from the port: other keys, comments, an old key line and a line of an input the screen
     * does not own (touchpad_right). */
    WriteFile(path,
              "# launcher\n"
              "upscaler=fsr3\r\n"
              "key.cross=Space\n"
              "key.touchpad_right=Backspace\n"
              "pad.circle=a\n");

    assert(PesIniSaveBindings(path, keys, pads) == 1);
    char *text = ReadFile(path);
    assert(strstr(text, "# launcher\n") != NULL);
    assert(strstr(text, "upscaler=fsr3") != NULL);
    assert(strstr(text, "key.touchpad_right=Backspace\n") != NULL);
    assert(strstr(text, "key.cross=Space") == NULL);           /* replaced */
    assert(strstr(text, "key.cross=X\n") != NULL);             /* slot A = cross, keyboard default X */
    assert(strstr(text, "key.square=A\n") != NULL);            /* slot X = square */
    assert(strstr(text, "key.up=Keypad 8\n") != NULL);
    assert(strstr(text, "key.l3=\n") != NULL);                 /* keyboard LS press unbound */
    assert(strstr(text, "pad.cross=a\n") != NULL);             /* gamepad A = cross */
    assert(strstr(text, "pad.square=x\n") != NULL);
    assert(strstr(text, "pad.l2=lefttrigger\n") != NULL);
    assert(strstr(text, "pad.touchpad=back\n") != NULL);
    assert(strstr(text, "pad.move_up") == NULL);               /* analog: no pad line */
    assert(strstr(text, "pad.look_left") == NULL);
    assert(strstr(text, "pad.circle=a\n") == NULL);            /* replaced by the screen's value */
    free(text);

    /* Read back: the same keyboard and gamepad bindings. */
    int back[PES_PAD_SLOTS];
    char back_pads[PES_PAD_SLOTS][PES_PAD_NAME_MAX];
    assert(PesIniLoadBindings(path, back, back_pads) == 3);
    for (int s = 0; s < PES_PAD_SLOTS; ++s) assert(back[s] == keys[s]);
    for (int s = 0; s < PES_PAD_SLOTS; ++s) {
        if (PesPadBindable(s)) assert(strcmp(back_pads[s], pads[s]) == 0);
    }

    /* A changed binding and an unbound slot survive a save and a load. */
    assert(PesKeyboardBind(keys, PES_SLOT_RS_UP, 'R') == 1);
    assert(PesKeyboardBind(keys, PES_SLOT_LS_PRESS, 0) == 1);
    assert(PesPadBind(pads, PES_SLOT_Y, "") == 1);
    assert(PesIniSaveBindings(path, keys, pads) == 1);
    assert(PesIniLoadBindings(path, back, back_pads) == 3);
    assert(back[PES_SLOT_RS_UP] == 'R' && back[PES_SLOT_LS_PRESS] == 0);
    assert(back_pads[PES_SLOT_Y][0] == '\0');
    text = ReadFile(path);
    assert(strstr(text, "key.look_up=R\n") != NULL);
    assert(strstr(text, "key.touchpad_right=Backspace\n") != NULL);
    assert(strstr(text, "# launcher\n") == text);
    free(text);

    /* A file with none of our lines loads nothing and leaves the caller's values alone. */
    WriteFile(path, "upscaler=fsr3\npad.circle_other=a\n");
    int kept[PES_PAD_SLOTS];
    PesKeyboardDefaults(kept);
    assert(PesIniLoadBindings(path, kept, back_pads) == 0);
    assert(kept[PES_SLOT_X] == 'A' && kept[PES_SLOT_RS_UP] == 0);

    /* Pad lines only: keys stay, pads load. */
    WriteFile(path, "pad.cross=b\n");
    PesKeyboardDefaults(kept);
    assert(PesIniLoadBindings(path, kept, back_pads) == 2);
    assert(kept[PES_SLOT_X] == 'A');
    assert(strcmp(back_pads[PES_SLOT_A], "b") == 0);

    /* A missing file loads nothing; saving creates it. */
    remove(path);
    assert(PesIniLoadBindings(path, kept, back_pads) == 0);
    PesKeyboardDefaults(kept);
    PesPadDefaults(pads);
    assert(PesIniSaveBindings(path, kept, pads) == 1);
    assert(PesIniLoadBindings(path, back, back_pads) == 3);
    for (int s = 0; s < PES_PAD_SLOTS; ++s) assert(back[s] == kept[s]);

    /* The input method: written as one pad_input line, replaced on the next save, read back. */
    int method = -1;
    assert(PesIniLoadPadMethod(path, &method) == 0);           /* no line yet */
    assert(method == -1);
    assert(PesIniSavePadMethod(path, PES_PAD_DIRECTINPUT) == 1);
    assert(PesIniLoadPadMethod(path, &method) == 1);
    assert(method == PES_PAD_DIRECTINPUT);
    assert(PesIniSavePadMethod(path, PES_PAD_XINPUT) == 1);
    assert(PesIniLoadPadMethod(path, &method) == 1);
    assert(method == PES_PAD_XINPUT);
    text = ReadFile(path);
    assert(strstr(text, "pad_input=xinput\n") != NULL);
    assert(strstr(text, "pad_input=directinput") == NULL);     /* replaced, not appended */
    assert(strstr(text, "key.cross=X\n") != NULL);             /* bindings kept */
    free(text);

    /* A whole-number setting: saved, replaced, read back; other lines kept. */
    WriteFile(path, "upscaler=fsr3\nkey.cross=X\n");
    int audio = 0;
    assert(PesIniLoadInt(path, "audio_buffer", &audio) == 0 && audio == 0);
    assert(PesIniSaveInt(path, "audio_buffer", 3) == 1);
    assert(PesIniLoadInt(path, "audio_buffer", &audio) == 1 && audio == 3);
    assert(PesIniSaveInt(path, "audio_buffer", 7) == 1);
    assert(PesIniLoadInt(path, "audio_buffer", &audio) == 1 && audio == 7);
    text = ReadFile(path);
    assert(strstr(text, "audio_buffer=7\n") != NULL);
    assert(strstr(text, "audio_buffer=3") == NULL);            /* replaced, not appended */
    assert(strstr(text, "upscaler=fsr3\n") != NULL);
    assert(strstr(text, "key.cross=X\n") != NULL);
    free(text);

    remove(path);
    printf("PASS: keyboard and gamepad bindings saved and loaded, other settings kept, input method saved\n");
    return 0;
}
