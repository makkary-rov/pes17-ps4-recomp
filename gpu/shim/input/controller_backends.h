// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef PES_CONTROLLER_BACKENDS_H
#define PES_CONTROLLER_BACKENDS_H

#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

// Gamepad state keeps updating when the window is unfocused. Must run before SDL gamepad init,
// as both callers already do.
static inline void pes_controller_backends(void) {
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
}

#ifdef __cplusplus
}
#endif

#endif
