/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Settings model for the settings window (M6.5a): the values the Display tab edits, their allowed
 * sets and their clamping. Plain C with no Windows calls, so it is tested on every platform.
 * Keys and meanings match gpu/shim/pes_settings.cpp (display_mode, vsync, fps_ingame, fps_replay,
 * quality, audio_buffer, output_res). */
#ifndef PES_LAUNCHER_SETTINGS_MODEL_H
#define PES_LAUNCHER_SETTINGS_MODEL_H

enum PesDisplayMode { PES_MODE_WINDOWED = 0, PES_MODE_BORDERLESS = 1, PES_MODE_FULLSCREEN = 2 };
enum PesQuality { PES_QUALITY_LOW = 0, PES_QUALITY_MEDIUM = 1, PES_QUALITY_HIGH = 2 };

typedef struct {
    int display_mode;    /* enum PesDisplayMode */
    int resolution;      /* index into PesResolutions */
    int quality;         /* enum PesQuality: saved only, no effect yet */
    int vsync;           /* 0 or 1; on by default (PS4 default) */
    int fps_ingame;      /* 60 or 30 */
    int fps_replay;      /* 60 or 30 */
    int audio_buffer;    /* 1..8: saved only, no effect yet */
} PesDisplaySettings;

/* The fixed display list (user decision): 640x480 up to 1920x1080, in size order. */
typedef struct { int width, height; const char *label; } PesResolution;
#define PES_RESOLUTION_COUNT 7
extern const PesResolution PesResolutions[PES_RESOLUTION_COUNT];

/* The default values for a new settings file. */
PesDisplaySettings PesDisplayDefaults(void);

/* Clamps every field to its allowed set; invalid values fall back to the default for that field. */
PesDisplaySettings PesDisplayClamp(PesDisplaySettings s);

/* Index of the resolution with this size, or -1. */
int PesResolutionIndex(int width, int height);

/* Text for a display mode, as saved in pes17recomp.ini: "windowed", "borderless" or "fullscreen". */
const char *PesDisplayModeName(int mode);

#endif
