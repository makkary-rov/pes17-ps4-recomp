/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "settings_display_ini.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "settings_ini.h"

/* The game's FSR preset numbers (pes_settings.h Preset: NativeAA 0, Quality 1, Balanced 2,
 * Performance 3, UltraPerformance 4) for the window's FSR modes (PesFsrPreset: 0 Ultra Performance
 * .. 3 Quality). */
static int GamePreset(int fsr_preset) {
    static const int kPreset[4] = {4, 3, 2, 1};
    return kPreset[fsr_preset >= 0 && fsr_preset <= 3 ? fsr_preset : PES_FSR_QUALITY];
}

static int FsrPreset(int game_preset) {
    switch (game_preset) {
    case 4: return PES_FSR_ULTRA_PERFORMANCE;
    case 3: return PES_FSR_PERFORMANCE;
    case 2: return PES_FSR_BALANCED;
    default: return PES_FSR_QUALITY; /* 1 Quality, and native AA, which the window has no mode for */
    }
}

static const char *QualityName(int quality) {
    static const char *kNames[3] = {"low", "medium", "high"};
    return kNames[quality >= 0 && quality <= 2 ? quality : PES_QUALITY_HIGH];
}

int PesIniSaveDisplay(const char *path, PesDisplaySettings s) {
    s = PesDisplayClamp(s);
    const int fallback = PesResolutionIndex(1920, 1080);

    /* An upscaler the window does not offer stays while FSR is on, so OK does not undo the choice. */
    char current_upscaler[32] = "";
    const int has_upscaler = PesIniLoadValue(path, "upscaler", current_upscaler, sizeof(current_upscaler));
    const char *upscaler = "off";
    if (s.fsr_enabled) {
        const int offered = !has_upscaler || current_upscaler[0] == '\0' || !strcmp(current_upscaler, "off") ||
                            !strcmp(current_upscaler, "fsr3");
        upscaler = offered ? "fsr3" : current_upscaler;
    }

    /* A size outside the fixed list stays while the list shows its fallback (it was set in the menu). */
    char current_res[32] = "";
    int width = 0, height = 0;
    const int keep_res = PesIniLoadValue(path, "output_res", current_res, sizeof(current_res)) &&
                         sscanf(current_res, "%dx%d", &width, &height) == 2 &&
                         PesResolutionIndex(width, height) < 0 && s.resolution == fallback;

    char values[16][32];
    const char *keys[16];
    const char *vals[16];
    int n = 0;
    keys[n] = "display_mode"; snprintf(values[n], 32, "%s", PesDisplayModeName(s.display_mode)); vals[n] = values[n]; ++n;
    keys[n] = "fullscreen"; snprintf(values[n], 32, "%d", s.display_mode != PES_MODE_WINDOWED ? 1 : 0); vals[n] = values[n]; ++n;
    if (!keep_res) {
        keys[n] = "output_res";
        snprintf(values[n], 32, "%dx%d", PesResolutions[s.resolution].width, PesResolutions[s.resolution].height);
        vals[n] = values[n];
        ++n;
    }
    keys[n] = "vsync"; snprintf(values[n], 32, "%d", s.vsync ? 1 : 0); vals[n] = values[n]; ++n;
    keys[n] = "fps_ingame"; snprintf(values[n], 32, "%d", s.fps_ingame); vals[n] = values[n]; ++n;
    keys[n] = "fps_replay"; snprintf(values[n], 32, "%d", s.fps_replay); vals[n] = values[n]; ++n;
    keys[n] = "quality"; snprintf(values[n], 32, "%s", QualityName(s.quality)); vals[n] = values[n]; ++n;
    /* The game's motion blur is an on/off effect (effect_motion_blur); the 25/50/75 steps are kept in
     * motion_blur for when the effect takes a strength. */
    keys[n] = "effect_motion_blur"; snprintf(values[n], 32, "%d", s.motion_blur != 0 ? 1 : 0); vals[n] = values[n]; ++n;
    keys[n] = "motion_blur"; snprintf(values[n], 32, "%d", s.motion_blur); vals[n] = values[n]; ++n;
    keys[n] = "upscaler"; snprintf(values[n], 32, "%s", upscaler); vals[n] = values[n]; ++n;
    keys[n] = "preset"; snprintf(values[n], 32, "%d", GamePreset(s.fsr_preset)); vals[n] = values[n]; ++n;

    return PesIniSaveValues(path, keys, vals, n);
}

int PesIniLoadDisplay(const char *path, PesDisplaySettings *s) {
    char v[64];
    if (PesIniLoadValue(path, "display_mode", v, sizeof(v))) {
        if (!strcmp(v, "windowed")) s->display_mode = PES_MODE_WINDOWED;
        else if (!strcmp(v, "borderless")) s->display_mode = PES_MODE_BORDERLESS;
        else if (!strcmp(v, "fullscreen")) s->display_mode = PES_MODE_FULLSCREEN;
    } else if (PesIniLoadValue(path, "fullscreen", v, sizeof(v))) {
        /* An older file (fullscreen only) means borderless, as the game reads it. */
        s->display_mode = atoi(v) != 0 ? PES_MODE_BORDERLESS : PES_MODE_WINDOWED;
    }

    int width = 0, height = 0;
    if (PesIniLoadValue(path, "output_res", v, sizeof(v)) && sscanf(v, "%dx%d", &width, &height) == 2) {
        const int index = PesResolutionIndex(width, height);
        s->resolution = index >= 0 ? index : PesResolutionIndex(1920, 1080);
    }

    if (PesIniLoadValue(path, "vsync", v, sizeof(v))) s->vsync = atoi(v) != 0;
    if (PesIniLoadValue(path, "fps_ingame", v, sizeof(v))) {
        const int fps = atoi(v);
        if (fps == 120 || fps == 60 || fps == 30) s->fps_ingame = fps;
    }
    if (PesIniLoadValue(path, "fps_replay", v, sizeof(v))) {
        const int fps = atoi(v);
        if (fps == 60 || fps == 30) s->fps_replay = fps;
    }
    if (PesIniLoadValue(path, "quality", v, sizeof(v))) {
        if (!strcmp(v, "low") || !strcmp(v, "0")) s->quality = PES_QUALITY_LOW;
        else if (!strcmp(v, "medium") || !strcmp(v, "1")) s->quality = PES_QUALITY_MEDIUM;
        else if (!strcmp(v, "high") || !strcmp(v, "2")) s->quality = PES_QUALITY_HIGH;
    }
    /* The strength steps are the motion_blur key; without it, the game's on/off effect decides. */
    if (PesIniLoadValue(path, "motion_blur", v, sizeof(v))) {
        s->motion_blur = atoi(v);
    } else if (PesIniLoadValue(path, "effect_motion_blur", v, sizeof(v))) {
        s->motion_blur = atoi(v) != 0 ? 100 : 0;
    }
    if (PesIniLoadValue(path, "upscaler", v, sizeof(v))) s->fsr_enabled = v[0] != '\0' && strcmp(v, "off") != 0;
    if (PesIniLoadValue(path, "preset", v, sizeof(v))) s->fsr_preset = FsrPreset(atoi(v));
    return 1;
}
