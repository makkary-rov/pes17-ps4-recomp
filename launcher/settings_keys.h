/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Keyboard bindings of the settings window (M6.5a, Keyboard tab): the 24 input slots, their default
 * keys and the rule that one key is bound to one slot at a time. Plain C, no Windows calls, so it is
 * tested on every platform. Keys are Windows virtual-key codes (VK_*); 0 means unbound.
 *
 * Slot order follows the reference screenshot and the 2017 Settings_b.dll (the default table):
 * 0 X, 1 A, 2 B, 3 Y, 4 LT, 5 RT, 6 LB, 7 RB, 8 Start, 9 Back, 10 LS press, 11 RS press,
 * 12-15 d-pad Up/Down/Left/Right, 16-19 LS Up/Down/Left/Right, 20-23 RS Up/Down/Left/Right. */
#ifndef PES_LAUNCHER_SETTINGS_KEYS_H
#define PES_LAUNCHER_SETTINGS_KEYS_H

#include <stddef.h>

#define PES_PAD_SLOTS 24

enum PesPadSlot {
    PES_SLOT_X = 0, PES_SLOT_A, PES_SLOT_B, PES_SLOT_Y, PES_SLOT_LT, PES_SLOT_RT, PES_SLOT_LB, PES_SLOT_RB,
    PES_SLOT_START, PES_SLOT_BACK, PES_SLOT_LS_PRESS, PES_SLOT_RS_PRESS,
    PES_SLOT_DPAD_UP, PES_SLOT_DPAD_DOWN, PES_SLOT_DPAD_LEFT, PES_SLOT_DPAD_RIGHT,
    PES_SLOT_LS_UP, PES_SLOT_LS_DOWN, PES_SLOT_LS_LEFT, PES_SLOT_LS_RIGHT,
    PES_SLOT_RS_UP, PES_SLOT_RS_DOWN, PES_SLOT_RS_LEFT, PES_SLOT_RS_RIGHT,
};

/* Short label of a slot as shown on the tab (e.g. "LT", "Up"). */
const char *PesPadSlotLabel(int slot);

/* The defaults from the 2017 Settings_b.dll key table. */
void PesKeyboardDefaults(int keys[PES_PAD_SLOTS]);

/* Binds key to slot. The key is removed from any other slot first (one key, one slot); key 0 unbinds
 * the slot. Returns 0 for an out-of-range slot or a key the game cannot read, 1 otherwise. */
int PesKeyboardBind(int keys[PES_PAD_SLOTS], int slot, int key);

/* The game's input name for a slot in the settings file (key.<name>=), e.g. "square" for X. */
const char *PesSlotInputName(int slot);

/* The SDL key name the game reads for a virtual key (e.g. "Keypad 8", "Return", "Z"), in buf.
 * Returns 1 if the key can be bound, 0 if it cannot (then buf is empty). */
int PesKeySdlName(int key, char *buf, size_t size);

/* The virtual key for an SDL key name from the settings file, or 0 if unknown. */
int PesKeyFromSdlName(const char *name);

/* Writes the name of a virtual key into buf (NUL-terminated). Unknown keys become "Key 0xNN". */
void PesKeyName(int key, char *buf, size_t size);

#endif
