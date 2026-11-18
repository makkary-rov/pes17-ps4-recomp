/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "settings_model.h"

const PesResolution PesResolutions[PES_RESOLUTION_COUNT] = {
    {640, 480, "(640x480) 4:3"},   {800, 600, "(800x600) 4:3"},   {1024, 768, "(1024x768) 4:3"},
    {1280, 720, "(1280x720) 16:9"}, {1366, 768, "(1366x768) 16:9"}, {1600, 900, "(1600x900) 16:9"},
    {1920, 1080, "(1920x1080) 16:9"},
};

const int PesMotionBlurSteps[PES_MOTION_BLUR_COUNT] = {0, 25, 50, 75, 100};

PesDisplaySettings PesDisplayDefaults(void) {
    PesDisplaySettings s;
    s.display_mode = PES_MODE_WINDOWED;
    s.resolution = PesResolutionIndex(1920, 1080);
    s.quality = PES_QUALITY_HIGH;
    s.vsync = 1;
    s.fps_ingame = 60;
    s.fps_replay = 30;
    s.audio_buffer = 1;
    s.motion_blur = 0;
    s.fsr_enabled = 0;
    s.fsr_preset = PES_FSR_QUALITY;
    return s;
}

static int InRange(int v, int lo, int hi) { return v >= lo && v <= hi; }

PesDisplaySettings PesDisplayClamp(PesDisplaySettings s) {
    const PesDisplaySettings d = PesDisplayDefaults();
    if (!InRange(s.display_mode, PES_MODE_WINDOWED, PES_MODE_FULLSCREEN)) s.display_mode = d.display_mode;
    if (!InRange(s.resolution, 0, PES_RESOLUTION_COUNT - 1)) s.resolution = d.resolution;
    if (!InRange(s.quality, PES_QUALITY_LOW, PES_QUALITY_HIGH)) s.quality = d.quality;
    s.vsync = s.vsync ? 1 : 0;
    if (s.fps_ingame != 60 && s.fps_ingame != 30) s.fps_ingame = d.fps_ingame;
    if (s.fps_replay != 60 && s.fps_replay != 30) s.fps_replay = d.fps_replay;
    if (!InRange(s.audio_buffer, 1, 8)) s.audio_buffer = d.audio_buffer;
    if (s.motion_blur != 0 && s.motion_blur != 25 && s.motion_blur != 50 && s.motion_blur != 75 && s.motion_blur != 100) s.motion_blur = d.motion_blur;
    s.fsr_enabled = s.fsr_enabled ? 1 : 0;
    if (!InRange(s.fsr_preset, PES_FSR_ULTRA_PERFORMANCE, PES_FSR_QUALITY)) s.fsr_preset = d.fsr_preset;
    return s;
}

int PesResolutionIndex(int width, int height) {
    for (int i = 0; i < PES_RESOLUTION_COUNT; ++i) {
        if (PesResolutions[i].width == width && PesResolutions[i].height == height) return i;
    }
    return -1;
}

const char *PesDisplayModeName(int mode) {
    switch (mode) {
    case PES_MODE_BORDERLESS: return "borderless";
    case PES_MODE_FULLSCREEN: return "fullscreen";
    default: return "windowed";
    }
}
