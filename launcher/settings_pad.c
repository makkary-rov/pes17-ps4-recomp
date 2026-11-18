/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "settings_pad.h"
#include <stdio.h>
#include <string.h>

/* The port's gamepad defaults (runtime_pad.c bind_defaults), by slot:
 * X=square(west) A=cross(south) B=circle(east) Y=triangle(north) LT=l2 RT=r2 LB=l1 RB=r1
 * Start=options Back=touchpad LS=l3 RS=r3, d-pad. The stick directions have no button. */
static const char *const kDefaults[PES_PAD_SLOTS] = {
    "x", "a", "b", "y", "lefttrigger", "righttrigger", "leftshoulder", "rightshoulder",
    "start", "back", "leftstick", "rightstick",
    "dpup", "dpdown", "dpleft", "dpright",
    "", "", "", "",
    "", "", "", "",
};

void PesPadDefaults(char names[PES_PAD_SLOTS][PES_PAD_NAME_MAX]) {
    for (int s = 0; s < PES_PAD_SLOTS; ++s) {
        strncpy(names[s], kDefaults[s], PES_PAD_NAME_MAX - 1);
        names[s][PES_PAD_NAME_MAX - 1] = '\0';
    }
}

int PesPadBindable(int slot) {
    return slot >= 0 && slot < PES_SLOT_LS_UP;
}

int PesPadBind(char names[PES_PAD_SLOTS][PES_PAD_NAME_MAX], int slot, const char *name) {
    if (slot < 0 || slot >= PES_PAD_SLOTS || !PesPadBindable(slot)) return 0;
    if (!name) name = "";
    if (strlen(name) >= PES_PAD_NAME_MAX) return 0;
    if (name[0]) {
        for (int s = 0; s < PES_PAD_SLOTS; ++s) {
            if (strcmp(names[s], name) == 0) names[s][0] = '\0';
        }
    }
    strncpy(names[slot], name, PES_PAD_NAME_MAX - 1);
    names[slot][PES_PAD_NAME_MAX - 1] = '\0';
    return 1;
}

void PesPadLabel(const char *name, char *buf, size_t size) {
    static const struct { const char *name, *label; } kLabels[] = {
        {"a", "A"}, {"b", "B"}, {"x", "X"}, {"y", "Y"},
        {"leftshoulder", "LB"}, {"rightshoulder", "RB"}, {"lefttrigger", "LT"}, {"righttrigger", "RT"},
        {"back", "Back"}, {"start", "Start"}, {"leftstick", "LS press"}, {"rightstick", "RS press"},
        {"dpup", "Up"}, {"dpdown", "Down"}, {"dpleft", "Left"}, {"dpright", "Right"},
    };
    if (size == 0) return;
    buf[0] = '\0';
    if (!name || !name[0]) return;
    for (size_t i = 0; i < sizeof(kLabels) / sizeof(kLabels[0]); ++i) {
        if (strcmp(name, kLabels[i].name) == 0) {
            snprintf(buf, size, "%s", kLabels[i].label);
            return;
        }
    }
    snprintf(buf, size, "%s", name);
}

/* XInput wButtons bits. */
enum {
    XI_DPAD_UP = 0x0001, XI_DPAD_DOWN = 0x0002, XI_DPAD_LEFT = 0x0004, XI_DPAD_RIGHT = 0x0008,
    XI_START = 0x0010, XI_BACK = 0x0020, XI_LEFT_THUMB = 0x0040, XI_RIGHT_THUMB = 0x0080,
    XI_LEFT_SHOULDER = 0x0100, XI_RIGHT_SHOULDER = 0x0200, XI_A = 0x1000, XI_B = 0x2000,
    XI_X = 0x4000, XI_Y = 0x8000,
};

const char *PesPadNameFromXInput(unsigned buttons, unsigned left_trigger, unsigned right_trigger) {
    static const struct { unsigned bit; const char *name; } kBits[] = {
        {XI_A, "a"}, {XI_B, "b"}, {XI_X, "x"}, {XI_Y, "y"},
        {XI_LEFT_SHOULDER, "leftshoulder"}, {XI_RIGHT_SHOULDER, "rightshoulder"},
        {XI_BACK, "back"}, {XI_START, "start"}, {XI_LEFT_THUMB, "leftstick"}, {XI_RIGHT_THUMB, "rightstick"},
        {XI_DPAD_UP, "dpup"}, {XI_DPAD_DOWN, "dpdown"}, {XI_DPAD_LEFT, "dpleft"}, {XI_DPAD_RIGHT, "dpright"},
    };
    for (size_t i = 0; i < sizeof(kBits) / sizeof(kBits[0]); ++i) {
        if (buttons & kBits[i].bit) return kBits[i].name;
    }
    /* Triggers count as pressed past half travel. */
    if (left_trigger > 127) return "lefttrigger";
    if (right_trigger > 127) return "righttrigger";
    return "";
}
