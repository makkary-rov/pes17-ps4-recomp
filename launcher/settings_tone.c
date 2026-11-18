/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "settings_tone.h"
#include <math.h>

#define TONE_RATE 48000
#define TONE_CHANNELS 2
#define TONE_SECONDS 1
#define TONE_FRAMES (TONE_RATE * TONE_SECONDS)
#define TONE_DATA_BYTES (TONE_FRAMES * TONE_CHANNELS * 2)
#define TONE_HEADER 44
#define TONE_FADE (TONE_RATE / 50) /* 20 ms in and out, so the tone does not click */
#define TONE_AMPLITUDE 0.25        /* about -12 dBFS: a test level, not a loud one */
#define TONE_PI 3.14159265358979323846

/* Frequency, and the level of the tone on the left and on the right of the stereo output. */
static const struct { double hz, left, right; } kSpeakers[PES_SPK_COUNT] = {
    {440.0, 1.0, 0.0},       /* L: left only */
    {660.0, 0.0, 1.0},       /* R: right only */
    {550.0, 0.7071, 0.7071}, /* C: both sides */
    {60.0, 0.7071, 0.7071},  /* Lfe: both sides, low */
    {330.0, 0.7071, 0.0},    /* Ls: left side */
    {880.0, 0.0, 0.7071},    /* Rs: right side */
};

static void Put16(unsigned char *p, unsigned v) {
    p[0] = (unsigned char)(v & 0xff);
    p[1] = (unsigned char)((v >> 8) & 0xff);
}

static void Put32(unsigned char *p, unsigned long v) {
    Put16(p, (unsigned)(v & 0xffff));
    Put16(p + 2, (unsigned)((v >> 16) & 0xffff));
}

size_t PesToneSize(void) {
    return TONE_HEADER + TONE_DATA_BYTES;
}

size_t PesToneWav(int speaker, unsigned char *out, size_t cap) {
    if (speaker < 0 || speaker >= PES_SPK_COUNT || !out || cap < PesToneSize()) return 0;

    unsigned char *h = out;
    const unsigned long block = TONE_CHANNELS * 2;
    /* RIFF/WAVE header: PCM, 16 bit, stereo, 48 kHz. */
    h[0] = 'R'; h[1] = 'I'; h[2] = 'F'; h[3] = 'F';
    Put32(h + 4, (unsigned long)(TONE_HEADER - 8 + TONE_DATA_BYTES));
    h[8] = 'W'; h[9] = 'A'; h[10] = 'V'; h[11] = 'E';
    h[12] = 'f'; h[13] = 'm'; h[14] = 't'; h[15] = ' ';
    Put32(h + 16, 16);
    Put16(h + 20, 1);                             /* PCM */
    Put16(h + 22, TONE_CHANNELS);
    Put32(h + 24, TONE_RATE);
    Put32(h + 28, TONE_RATE * block);             /* bytes per second */
    Put16(h + 32, (unsigned)block);               /* bytes per frame */
    Put16(h + 34, 16);                            /* bits per sample */
    h[36] = 'd'; h[37] = 'a'; h[38] = 't'; h[39] = 'a';
    Put32(h + 40, (unsigned long)TONE_DATA_BYTES);

    unsigned char *p = out + TONE_HEADER;
    const double hz = kSpeakers[speaker].hz;
    for (int i = 0; i < TONE_FRAMES; ++i) {
        double env = 1.0;
        if (i < TONE_FADE) env = (double)i / TONE_FADE;
        else if (i >= TONE_FRAMES - TONE_FADE) env = (double)(TONE_FRAMES - i) / TONE_FADE;
        const double s = sin(2.0 * TONE_PI * hz * i / TONE_RATE) * env * TONE_AMPLITUDE;
        const double levels[TONE_CHANNELS] = {kSpeakers[speaker].left, kSpeakers[speaker].right};
        for (int c = 0; c < TONE_CHANNELS; ++c) {
            const double v = s * levels[c] * 32767.0;
            const int q = (int)(v >= 0 ? v + 0.5 : v - 0.5);
            Put16(p, (unsigned)(q & 0xffff));
            p += 2;
        }
    }
    return TONE_HEADER + TONE_DATA_BYTES;
}
