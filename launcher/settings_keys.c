/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "settings_keys.h"
#include <stdio.h>
#include <string.h>

/* Windows virtual-key codes used by the defaults and names. */
enum {
    VK_BACK_KEY = 0x08, VK_TAB_KEY = 0x09, VK_RETURN_KEY = 0x0D, VK_SHIFT_KEY = 0x10, VK_CONTROL_KEY = 0x11,
    VK_MENU_KEY = 0x12, VK_ESCAPE_KEY = 0x1B, VK_SPACE_KEY = 0x20, VK_LEFT_KEY = 0x25, VK_UP_KEY = 0x26,
    VK_RIGHT_KEY = 0x27, VK_DOWN_KEY = 0x28, VK_NUMPAD0_KEY = 0x60, VK_NUMPAD2_KEY = 0x62,
    VK_NUMPAD4_KEY = 0x64, VK_NUMPAD6_KEY = 0x66, VK_NUMPAD8_KEY = 0x68,
};

static const char *const kSlotLabels[PES_PAD_SLOTS] = {
    "X", "A", "B", "Y", "LT", "RT", "LB", "RB", "Start", "Back", "LS press", "RS press",
    "Up", "Down", "Left", "Right",
    "LS Up", "LS Down", "LS Left", "LS Right",
    "RS Up", "RS Down", "RS Left", "RS Right",
};

/* The game's input names (runtime_pad.c input_names) for the 24 slots, in slot order. */
static const char *const kInputNames[PES_PAD_SLOTS] = {
    "square", "cross", "circle", "triangle", "l2", "r2", "l1", "r1",
    "options", "touchpad", "l3", "r3",
    "up", "down", "left", "right",
    "move_up", "move_down", "move_left", "move_right",
    "look_up", "look_down", "look_left", "look_right",
};

const char *PesSlotInputName(int slot) {
    return slot >= 0 && slot < PES_PAD_SLOTS ? kInputNames[slot] : "";
}

const char *PesPadSlotLabel(int slot) {
    return slot >= 0 && slot < PES_PAD_SLOTS ? kSlotLabels[slot] : "";
}

void PesKeyboardDefaults(int keys[PES_PAD_SLOTS]) {
    static const int defaults[PES_PAD_SLOTS] = {
        'A', 'X', 'D', 'W', 'Z', 'C', 'Q', 'E', 'V', 'B', 0, 0,        /* X A B Y LT RT LB RB Start Back, stick presses */
        VK_NUMPAD8_KEY, VK_NUMPAD2_KEY, VK_NUMPAD4_KEY, VK_NUMPAD6_KEY, /* d-pad Up Down Left Right */
        VK_UP_KEY, VK_DOWN_KEY, VK_LEFT_KEY, VK_RIGHT_KEY,              /* LS Up Down Left Right */
        0, 0, 0, 0,                                                     /* RS Up Down Left Right */
    };
    for (int i = 0; i < PES_PAD_SLOTS; ++i) keys[i] = defaults[i];
}

int PesKeySdlName(int key, char *buf, size_t size) {
    if (size) buf[0] = '\0';
    if (size == 0) return 0;
    if (key >= 'A' && key <= 'Z') { snprintf(buf, size, "%c", key); return 1; }
    if (key >= '0' && key <= '9') { snprintf(buf, size, "%c", key); return 1; }
    if (key >= VK_NUMPAD0_KEY && key <= VK_NUMPAD0_KEY + 9) { snprintf(buf, size, "Keypad %d", key - VK_NUMPAD0_KEY); return 1; }
    switch (key) {
    case VK_UP_KEY: snprintf(buf, size, "Up"); return 1;
    case VK_DOWN_KEY: snprintf(buf, size, "Down"); return 1;
    case VK_LEFT_KEY: snprintf(buf, size, "Left"); return 1;
    case VK_RIGHT_KEY: snprintf(buf, size, "Right"); return 1;
    case VK_SPACE_KEY: snprintf(buf, size, "Space"); return 1;
    case VK_RETURN_KEY: snprintf(buf, size, "Return"); return 1;
    case VK_ESCAPE_KEY: snprintf(buf, size, "Escape"); return 1;
    case VK_TAB_KEY: snprintf(buf, size, "Tab"); return 1;
    case VK_BACK_KEY: snprintf(buf, size, "Backspace"); return 1;
    case VK_SHIFT_KEY: snprintf(buf, size, "LShift"); return 1;
    case VK_CONTROL_KEY: snprintf(buf, size, "LCtrl"); return 1;
    case VK_MENU_KEY: snprintf(buf, size, "LAlt"); return 1;
    default: return 0;
    }
}

int PesKeyFromSdlName(const char *name) {
    char candidate[32];
    if (!name || !*name) return 0;
    for (int key = 1; key <= 0xFE; ++key) {
        if (PesKeySdlName(key, candidate, sizeof(candidate)) && strcmp(candidate, name) == 0) return key;
    }
    return 0;
}

int PesKeyboardBind(int keys[PES_PAD_SLOTS], int slot, int key) {
    if (slot < 0 || slot >= PES_PAD_SLOTS) return 0;
    if (key < 0 || key > 0xFE) return 0;
    if (key != 0) {
        char name[32];
        if (!PesKeySdlName(key, name, sizeof(name))) return 0;
    }
    if (key != 0) {
        for (int i = 0; i < PES_PAD_SLOTS; ++i) {
            if (keys[i] == key) keys[i] = 0;
        }
    }
    keys[slot] = key;
    return 1;
}

void PesKeyName(int key, char *buf, size_t size) {
    if (size == 0) return;
    if (key == 0) {
        buf[0] = '\0';
        return;
    }
    if ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9')) {
        snprintf(buf, size, "%c", key);
        return;
    }
    if (key >= VK_NUMPAD0_KEY && key <= VK_NUMPAD0_KEY + 9) {
        snprintf(buf, size, "Num %d", key - VK_NUMPAD0_KEY);
        return;
    }
    switch (key) {
    case VK_UP_KEY: snprintf(buf, size, "Up"); return;
    case VK_DOWN_KEY: snprintf(buf, size, "Down"); return;
    case VK_LEFT_KEY: snprintf(buf, size, "Left"); return;
    case VK_RIGHT_KEY: snprintf(buf, size, "Right"); return;
    case VK_SPACE_KEY: snprintf(buf, size, "Space"); return;
    case VK_RETURN_KEY: snprintf(buf, size, "Enter"); return;
    case VK_ESCAPE_KEY: snprintf(buf, size, "Esc"); return;
    case VK_TAB_KEY: snprintf(buf, size, "Tab"); return;
    case VK_BACK_KEY: snprintf(buf, size, "Backspace"); return;
    case VK_SHIFT_KEY: snprintf(buf, size, "Shift"); return;
    case VK_CONTROL_KEY: snprintf(buf, size, "Ctrl"); return;
    case VK_MENU_KEY: snprintf(buf, size, "Alt"); return;
    default: snprintf(buf, size, "Key 0x%02X", key); return;
    }
}
