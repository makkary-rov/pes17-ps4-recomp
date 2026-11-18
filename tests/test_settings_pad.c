// SPDX-License-Identifier: GPL-2.0-or-later
// Gamepad bindings of the settings window (launcher/settings_pad.c): the port's current defaults, the
// bindable slots (not the analog stick directions), one name per slot, labels, and XInput capture.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "settings_pad.h"

int main(void) {
    char names[PES_PAD_SLOTS][PES_PAD_NAME_MAX];
    PesPadDefaults(names);
    /* The port's current defaults. */
    assert(strcmp(names[PES_SLOT_X], "x") == 0 && strcmp(names[PES_SLOT_A], "a") == 0);
    assert(strcmp(names[PES_SLOT_LT], "lefttrigger") == 0 && strcmp(names[PES_SLOT_RB], "rightshoulder") == 0);
    assert(strcmp(names[PES_SLOT_START], "start") == 0 && strcmp(names[PES_SLOT_BACK], "back") == 0);
    assert(strcmp(names[PES_SLOT_DPAD_LEFT], "dpleft") == 0);
    /* The stick directions have no button. */
    for (int s = PES_SLOT_LS_UP; s < PES_PAD_SLOTS; ++s) assert(names[s][0] == '\0' && !PesPadBindable(s));
    assert(PesPadBindable(PES_SLOT_LS_PRESS) && PesPadBindable(PES_SLOT_RS_PRESS));

    /* One name, one slot; analog slots refuse a binding. */
    assert(PesPadBind(names, PES_SLOT_Y, "a") == 1);
    assert(names[PES_SLOT_Y][0] && strcmp(names[PES_SLOT_Y], "a") == 0 && names[PES_SLOT_A][0] == '\0');
    assert(PesPadBind(names, PES_SLOT_LS_UP, "y") == 0);
    assert(PesPadBind(names, PES_PAD_SLOTS, "y") == 0);
    assert(PesPadBind(names, PES_SLOT_RS_PRESS, "") == 1 && names[PES_SLOT_RS_PRESS][0] == '\0');

    /* Labels on the button. */
    char label[32];
    PesPadLabel("leftshoulder", label, sizeof(label)); assert(strcmp(label, "LB") == 0);
    PesPadLabel("dpup", label, sizeof(label)); assert(strcmp(label, "Up") == 0);
    PesPadLabel("", label, sizeof(label)); assert(label[0] == '\0');
    PesPadLabel("someother", label, sizeof(label)); assert(strcmp(label, "someother") == 0);

    /* XInput capture: buttons first, then triggers past half travel. */
    assert(strcmp(PesPadNameFromXInput(0x1000, 0, 0), "a") == 0);
    assert(strcmp(PesPadNameFromXInput(0x0200, 0, 0), "rightshoulder") == 0);
    assert(strcmp(PesPadNameFromXInput(0, 200, 0), "lefttrigger") == 0);
    assert(strcmp(PesPadNameFromXInput(0, 0, 255), "righttrigger") == 0);
    assert(strcmp(PesPadNameFromXInput(0, 100, 100), "") == 0);
    assert(strcmp(PesPadNameFromXInput(0, 0, 0), "") == 0);

    printf("PASS: gamepad slots, port defaults, one name per slot, labels, XInput capture\n");
    return 0;
}
