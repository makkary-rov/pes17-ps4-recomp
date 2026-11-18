// SPDX-License-Identifier: GPL-2.0-or-later
// The Display tab's settings in the settings file (launcher/settings_display_ini.c): the values are written
// as the game reads them, read back as the window shows them, the two values the window cannot show
// (an upscaler other than FSR 3.1, an output size outside the list) are kept, and other lines stay.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "settings_display_ini.h"
#include "settings_ini.h"
#include "settings_model.h"

static char *ReadFile(const char *path) {
    FILE *f = fopen(path, "rb");
    assert(f);
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    assert(buf);
    assert(fread(buf, 1, (size_t)n, f) == (size_t)n);
    fclose(f);
    buf[n] = '\0';
    return buf;
}

static void WriteFile(const char *path, const char *text) {
    FILE *f = fopen(path, "wb");
    assert(f);
    fputs(text, f);
    fclose(f);
}

static int Same(PesDisplaySettings a, PesDisplaySettings b) {
    return a.display_mode == b.display_mode && a.resolution == b.resolution && a.quality == b.quality &&
           a.vsync == b.vsync && a.fps_ingame == b.fps_ingame && a.fps_replay == b.fps_replay &&
           a.audio_buffer == b.audio_buffer && a.motion_blur == b.motion_blur &&
           a.fsr_enabled == b.fsr_enabled && a.fsr_preset == b.fsr_preset;
}

int main(void) {
    const char *path = "test_settings_display_file.ini";
    remove(path);

    /* A new file: the defaults are written, and read back unchanged. */
    PesDisplaySettings s = PesDisplayDefaults();
    assert(PesIniSaveDisplay(path, s) == 1);
    PesDisplaySettings back = PesDisplayDefaults();
    back.display_mode = PES_MODE_FULLSCREEN;
    assert(PesIniLoadDisplay(path, &back) == 1);
    assert(Same(back, PesDisplayDefaults()));

    /* The lines the game reads: display mode, fullscreen (its old key), size, vsync, frame rates, quality,
     * motion blur, upscaler (off: FSR is off by default) and preset (Quality = 1 in the game's numbers). */
    char *text = ReadFile(path);
    assert(strstr(text, "display_mode=windowed\n") != NULL);
    assert(strstr(text, "fullscreen=0\n") != NULL);
    assert(strstr(text, "output_res=1920x1080\n") != NULL);
    assert(strstr(text, "vsync=1\n") != NULL);
    assert(strstr(text, "fps_ingame=60\n") != NULL);
    assert(strstr(text, "fps_replay=30\n") != NULL);
    assert(strstr(text, "quality=high\n") != NULL);
    assert(strstr(text, "effect_motion_blur=1\n") != NULL);
    assert(strstr(text, "motion_blur=100\n") != NULL);
    assert(strstr(text, "upscaler=off\n") != NULL);
    assert(strstr(text, "preset=1\n") != NULL);
    free(text);

    /* Every choice round-trips: borderless at 1280x720, vsync off, 30 fps in game, 60 in replays, low
     * quality, motion blur 50%, FSR 3.1 on in Performance mode. */
    s.display_mode = PES_MODE_BORDERLESS;
    s.resolution = PesResolutionIndex(1280, 720);
    s.vsync = 0;
    s.fps_ingame = 30;
    s.fps_replay = 60;
    s.quality = PES_QUALITY_LOW;
    s.motion_blur = 50;
    s.fsr_enabled = 1;
    s.fsr_preset = PES_FSR_PERFORMANCE;
    assert(PesIniSaveDisplay(path, s) == 1);
    text = ReadFile(path);
    assert(strstr(text, "display_mode=borderless\n") != NULL);
    assert(strstr(text, "fullscreen=1\n") != NULL);
    assert(strstr(text, "output_res=1280x720\n") != NULL);
    assert(strstr(text, "vsync=0\n") != NULL);
    assert(strstr(text, "fps_ingame=30\n") != NULL);
    assert(strstr(text, "quality=low\n") != NULL);
    assert(strstr(text, "effect_motion_blur=1\n") != NULL);   /* 50% is not off */
    assert(strstr(text, "motion_blur=50\n") != NULL);
    assert(strstr(text, "upscaler=fsr3\n") != NULL);
    assert(strstr(text, "preset=3\n") != NULL);                /* Performance = 3 in the game */
    free(text);
    PesDisplaySettings got = PesDisplayDefaults();
    assert(PesIniLoadDisplay(path, &got) == 1);
    assert(Same(got, s));

    /* FSR off: the upscaler is off; the chosen mode is kept for the next time FSR is on. */
    s.fsr_enabled = 0;
    assert(PesIniSaveDisplay(path, s) == 1);
    text = ReadFile(path);
    assert(strstr(text, "upscaler=off\n") != NULL);
    assert(strstr(text, "preset=3\n") != NULL);
    free(text);
    got = PesDisplayDefaults();
    assert(PesIniLoadDisplay(path, &got) == 1 && got.fsr_enabled == 0 && got.fsr_preset == PES_FSR_PERFORMANCE);

    /* An upscaler chosen in the in-game menu (FSR 4) stays while FSR is on; it reads back as on. */
    WriteFile(path, "# menu\nupscaler=fsr4\npreset=4\n");
    got = PesDisplayDefaults();
    assert(PesIniLoadDisplay(path, &got) == 1);
    assert(got.fsr_enabled == 1 && got.fsr_preset == PES_FSR_ULTRA_PERFORMANCE);
    s = got;
    assert(PesIniSaveDisplay(path, s) == 1);
    text = ReadFile(path);
    assert(strstr(text, "upscaler=fsr4\n") != NULL);
    assert(strstr(text, "# menu\n") == text);                  /* other lines kept */
    free(text);
    /* Turning FSR off replaces it. */
    s.fsr_enabled = 0;
    assert(PesIniSaveDisplay(path, s) == 1);
    text = ReadFile(path);
    assert(strstr(text, "upscaler=off\n") != NULL && strstr(text, "upscaler=fsr4") == NULL);
    free(text);

    /* A size outside the list (set in the menu) stays while the window shows its fallback. */
    WriteFile(path, "output_res=2560x1440\n");
    got = PesDisplayDefaults();
    assert(PesIniLoadDisplay(path, &got) == 1);
    assert(got.resolution == PesResolutionIndex(1920, 1080));
    assert(PesIniSaveDisplay(path, got) == 1);
    text = ReadFile(path);
    assert(strstr(text, "output_res=2560x1440\n") != NULL);
    free(text);
    /* A size the user picks replaces it. */
    got.resolution = PesResolutionIndex(1024, 768);
    assert(PesIniSaveDisplay(path, got) == 1);
    text = ReadFile(path);
    assert(strstr(text, "output_res=1024x768\n") != NULL && strstr(text, "2560x1440") == NULL);
    free(text);

    /* An older file with only fullscreen=1 means borderless, as the game reads it. */
    WriteFile(path, "fullscreen=1\n");
    got = PesDisplayDefaults();
    assert(PesIniLoadDisplay(path, &got) == 1 && got.display_mode == PES_MODE_BORDERLESS);

    /* Motion blur without its strength key follows the game's on/off effect. */
    WriteFile(path, "effect_motion_blur=0\n");
    got = PesDisplayDefaults();
    assert(PesIniLoadDisplay(path, &got) == 1 && got.motion_blur == 0);
    WriteFile(path, "effect_motion_blur=1\n");
    got = PesDisplayDefaults();
    assert(PesIniLoadDisplay(path, &got) == 1 && got.motion_blur == 100);

    /* A repeated line is collapsed, and the bindings and other settings in the file stay. */
    WriteFile(path, "key.cross=X\nvsync=1\nvsync=0\nupscaler=fsr3\nquality=medium\n");
    got = PesDisplayDefaults();
    assert(PesIniLoadDisplay(path, &got) == 1);
    assert(got.vsync == 0 && got.quality == PES_QUALITY_MEDIUM);
    assert(PesIniSaveDisplay(path, got) == 1);
    text = ReadFile(path);
    assert(strstr(text, "key.cross=X\n") != NULL);
    int vsync_lines = 0;
    for (const char *p = text; (p = strstr(p, "vsync=")) != NULL; p += 6) ++vsync_lines;
    assert(vsync_lines == 1);
    assert(strstr(text, "vsync=0\n") != NULL);
    free(text);

    remove(path);
    printf("PASS: display settings written as the game reads them, read back, two kept values, other lines kept\n");
    return 0;
}
