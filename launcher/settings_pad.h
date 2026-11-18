/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Gamepad bindings of the settings window (M6.5a, Controller tab): the 24 slots, the port's current
 * gamepad defaults (runtime_pad.c bind_defaults), which slots take a button, and the names the game
 * reads (SDL gamepad names: a b x y leftshoulder rightshoulder lefttrigger righttrigger back start
 * leftstick rightstick dpup dpdown dpleft dpright). Plain C, tested on every platform.
 *
 * The left and right stick directions (slots 16-23) are analog movement in the game, not buttons, so
 * they are shown but cannot be bound. Slots follow settings_keys.h. */
#ifndef PES_LAUNCHER_SETTINGS_PAD_H
#define PES_LAUNCHER_SETTINGS_PAD_H

#include <stddef.h>
#include "settings_keys.h"

#define PES_PAD_NAME_MAX 24

/* How the gamepad bindings are chosen: XInput (the default, the fixed standard mapping, not editable)
 * or DirectInput (manual mapping: the binding buttons can be set). */
enum PesPadMethod { PES_PAD_XINPUT = 0, PES_PAD_DIRECTINPUT = 1 };

/* The current gamepad defaults: one SDL name per slot, "" where a slot has none. */
void PesPadDefaults(char names[PES_PAD_SLOTS][PES_PAD_NAME_MAX]);

/* 1 if the slot can take a gamepad button (not the analog stick directions), else 0. */
int PesPadBindable(int slot);

/* Binds a gamepad name to a slot (removing it from any other slot first). An empty name unbinds.
 * Returns 0 for an out-of-range or analog slot, 1 otherwise. */
int PesPadBind(char names[PES_PAD_SLOTS][PES_PAD_NAME_MAX], int slot, const char *name);

/* Short text for a gamepad name on the button ("A", "LB", "Start", ...), or the name itself. */
void PesPadLabel(const char *name, char *buf, size_t size);

/* The gamepad name for the first pressed control of an XInput state, or "" if none. buttons is the
 * XInput wButtons word; the triggers are 0..255. */
const char *PesPadNameFromXInput(unsigned buttons, unsigned left_trigger, unsigned right_trigger);

#endif
