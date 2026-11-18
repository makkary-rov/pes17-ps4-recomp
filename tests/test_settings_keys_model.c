// SPDX-License-Identifier: GPL-2.0-or-later
// Keyboard bindings of the settings window (launcher/settings_keys.c): the 24 slots, the 2017 defaults,
// one key per slot, and key names.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "settings_keys.h"

int main(void) {
    int keys[PES_PAD_SLOTS];
    PesKeyboardDefaults(keys);
    /* Defaults from the 2017 table: X=A, A=X, B=D, Y=W, LT=Z, RT=C, LB=Q, RB=E, Start=V, Back=B. */
    assert(keys[PES_SLOT_X] == 'A' && keys[PES_SLOT_A] == 'X' && keys[PES_SLOT_B] == 'D' && keys[PES_SLOT_Y] == 'W');
    assert(keys[PES_SLOT_LT] == 'Z' && keys[PES_SLOT_RT] == 'C' && keys[PES_SLOT_LB] == 'Q' && keys[PES_SLOT_RB] == 'E');
    assert(keys[PES_SLOT_START] == 'V' && keys[PES_SLOT_BACK] == 'B');
    /* Stick presses and the right stick are unbound by default; d-pad on the numpad, left stick on arrows. */
    assert(keys[PES_SLOT_LS_PRESS] == 0 && keys[PES_SLOT_RS_PRESS] == 0);
    assert(keys[PES_SLOT_DPAD_UP] == 0x68 && keys[PES_SLOT_DPAD_DOWN] == 0x62);
    assert(keys[PES_SLOT_DPAD_LEFT] == 0x64 && keys[PES_SLOT_DPAD_RIGHT] == 0x66);
    assert(keys[PES_SLOT_LS_UP] == 0x26 && keys[PES_SLOT_LS_DOWN] == 0x28);
    assert(keys[PES_SLOT_LS_LEFT] == 0x25 && keys[PES_SLOT_LS_RIGHT] == 0x27);
    assert(keys[PES_SLOT_RS_UP] == 0 && keys[PES_SLOT_RS_DOWN] == 0 && keys[PES_SLOT_RS_LEFT] == 0 && keys[PES_SLOT_RS_RIGHT] == 0);

    /* Binding moves a key: the old slot is cleared, so one key is never on two slots. */
    assert(PesKeyboardBind(keys, PES_SLOT_LS_PRESS, 'F') == 1);
    assert(keys[PES_SLOT_LS_PRESS] == 'F');
    assert(PesKeyboardBind(keys, PES_SLOT_RS_PRESS, 'F') == 1);
    assert(keys[PES_SLOT_LS_PRESS] == 0 && keys[PES_SLOT_RS_PRESS] == 'F');

    /* Unbinding a slot with key 0. */
    assert(PesKeyboardBind(keys, PES_SLOT_RS_PRESS, 0) == 1 && keys[PES_SLOT_RS_PRESS] == 0);

    /* Out-of-range slots and keys are refused and change nothing. */
    int before[PES_PAD_SLOTS];
    memcpy(before, keys, sizeof(keys));
    assert(PesKeyboardBind(keys, -1, 'G') == 0 && PesKeyboardBind(keys, PES_PAD_SLOTS, 'G') == 0);
    assert(PesKeyboardBind(keys, 0, 0x1FF) == 0);
    assert(memcmp(before, keys, sizeof(keys)) == 0);

    /* Names. */
    char name[32];
    PesKeyName('Z', name, sizeof(name)); assert(strcmp(name, "Z") == 0);
    PesKeyName(0x68, name, sizeof(name)); assert(strcmp(name, "Num 8") == 0);
    PesKeyName(0x26, name, sizeof(name)); assert(strcmp(name, "Up") == 0);
    PesKeyName(0, name, sizeof(name)); assert(name[0] == '\0');
    PesKeyName(0x7B, name, sizeof(name)); assert(strcmp(name, "Key 0x7B") == 0);
    assert(strcmp(PesPadSlotLabel(PES_SLOT_LT), "LT") == 0);
    assert(strcmp(PesPadSlotLabel(PES_SLOT_RS_RIGHT), "RS Right") == 0);

    printf("PASS: keyboard slots, 2017 defaults, one key per slot, key names\n");
    return 0;
}
