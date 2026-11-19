// SPDX-License-Identifier: GPL-2.0-or-later
// Settings model of the settings window (launcher/settings_model.c): defaults, the fixed display list,
// clamping, and the saved mode names. Matches the keys in gpu/shim/pes_settings.cpp.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "settings_model.h"

int main(void) {
    /* The fixed display list: 640x480 up to 1920x1080, strictly increasing. */
    assert(PES_RESOLUTION_COUNT == 7);
    assert(PesResolutions[0].width == 640 && PesResolutions[0].height == 480);
    assert(PesResolutions[PES_RESOLUTION_COUNT - 1].width == 1920 && PesResolutions[PES_RESOLUTION_COUNT - 1].height == 1080);
    for (int i = 1; i < PES_RESOLUTION_COUNT; ++i) assert(PesResolutions[i].width > PesResolutions[i - 1].width);

    /* Defaults: windowed, 1920x1080, high quality, vsync on (PS4 default), 60 in game, 30 replays. */
    PesDisplaySettings d = PesDisplayDefaults();
    assert(d.display_mode == PES_MODE_WINDOWED);
    assert(PesResolutions[d.resolution].width == 1920);
    assert(d.quality == PES_QUALITY_HIGH && d.vsync == 1 && d.fps_ingame == 60 && d.fps_replay == 30);
    assert(d.audio_buffer == 1);
    assert(d.motion_blur == 100 && d.fsr_enabled == 0 && d.fsr_preset == PES_FSR_QUALITY); /* PS4 native blur, FSR off */

    /* Clamping: bad values fall back to the default for that field; good values stay. */
    PesDisplaySettings bad = {9, 99, 7, 5, 45, 12, 0, 40, 7, 9};
    PesDisplaySettings c = PesDisplayClamp(bad);
    assert(c.display_mode == PES_MODE_WINDOWED);
    assert(c.resolution == d.resolution);
    assert(c.quality == PES_QUALITY_HIGH);
    assert(c.vsync == 1);
    assert(c.fps_ingame == 60 && c.fps_replay == 30);
    PesDisplaySettings fast = PesDisplayDefaults();
    fast.fps_ingame = 120; /* not offered: falls back to 60 */
    fast.fps_replay = 120;
    fast = PesDisplayClamp(fast);
    assert(fast.fps_ingame == 60 && fast.fps_replay == 30); /* 30 or 60 only */
    assert(c.audio_buffer == 1);
    assert(c.motion_blur == 100 && c.fsr_preset == PES_FSR_QUALITY && c.fsr_enabled == 1);
    PesDisplaySettings good = {PES_MODE_FULLSCREEN, 0, PES_QUALITY_LOW, 0, 30, 30, 8, 75, 1, PES_FSR_BALANCED}; /* cutscenes: 30 only */
    PesDisplaySettings kept = PesDisplayClamp(good);
    assert(memcmp(&kept, &good, sizeof(good)) == 0);

    assert(PesMotionBlurSteps[4] == 100);
    /* Lookups. */
    assert(PesResolutionIndex(800, 600) == 1);
    assert(PesResolutionIndex(2560, 1440) == -1);
    assert(strcmp(PesDisplayModeName(PES_MODE_WINDOWED), "windowed") == 0);
    assert(strcmp(PesDisplayModeName(PES_MODE_BORDERLESS), "borderless") == 0);
    assert(strcmp(PesDisplayModeName(PES_MODE_FULLSCREEN), "fullscreen") == 0);

    printf("PASS: settings model: fixed display list, defaults, clamping, mode names\n");
    return 0;
}
