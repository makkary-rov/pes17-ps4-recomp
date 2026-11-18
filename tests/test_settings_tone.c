// SPDX-License-Identifier: GPL-2.0-or-later
// Audio test tones (launcher/settings_tone.c): a valid 16-bit stereo WAV for each speaker, the tone sits
// on the sides the speaker maps to, the level is the test level, and bad arguments write nothing.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "settings_tone.h"

static unsigned Get16(const unsigned char *p) {
    return (unsigned)p[0] | ((unsigned)p[1] << 8);
}

static unsigned long Get32(const unsigned char *p) {
    return (unsigned long)Get16(p) | ((unsigned long)Get16(p + 2) << 16);
}

/* Largest absolute sample on one channel (0 = left, 1 = right). */
static int Peak(const unsigned char *data, size_t frames, int channel) {
    int peak = 0;
    for (size_t i = 0; i < frames; ++i) {
        const short v = (short)Get16(data + (i * 2 + (size_t)channel) * 2);
        const int a = v < 0 ? -v : v;
        if (a > peak) peak = a;
    }
    return peak;
}

int main(void) {
    const size_t size = PesToneSize();
    assert(size == 44 + 48000 * 2 * 2);
    unsigned char *wav = malloc(size);
    assert(wav);

    /* Every speaker gives a valid WAV file of the same size. */
    for (int s = 0; s < PES_SPK_COUNT; ++s) {
        memset(wav, 0xaa, size);
        assert(PesToneWav(s, wav, size) == size);
        assert(memcmp(wav, "RIFF", 4) == 0 && memcmp(wav + 8, "WAVE", 4) == 0);
        assert(memcmp(wav + 12, "fmt ", 4) == 0 && memcmp(wav + 36, "data", 4) == 0);
        assert(Get16(wav + 20) == 1);                  /* PCM */
        assert(Get16(wav + 22) == 2);                  /* stereo */
        assert(Get32(wav + 24) == 48000);              /* sample rate */
        assert(Get16(wav + 34) == 16);                 /* bits per sample */
        assert(Get32(wav + 40) == size - 44);          /* data length */
        assert(Get32(wav + 4) == size - 8);            /* RIFF length */
    }

    const size_t frames = (size - 44) / 4;
    const int level = 32767 / 4; /* the test amplitude, 0.25 of full scale */

    /* L: left only; R: right only; Ls: left only; Rs: right only. */
    assert(PesToneWav(PES_SPK_L, wav, size) == size);
    assert(Peak(wav + 44, frames, 0) > level - 400 && Peak(wav + 44, frames, 0) <= level + 2);
    assert(Peak(wav + 44, frames, 1) == 0);

    assert(PesToneWav(PES_SPK_R, wav, size) == size);
    assert(Peak(wav + 44, frames, 0) == 0);
    assert(Peak(wav + 44, frames, 1) > level - 400);

    assert(PesToneWav(PES_SPK_LS, wav, size) == size);
    assert(Peak(wav + 44, frames, 0) > 0 && Peak(wav + 44, frames, 1) == 0);

    assert(PesToneWav(PES_SPK_RS, wav, size) == size);
    assert(Peak(wav + 44, frames, 0) == 0 && Peak(wav + 44, frames, 1) > 0);

    /* C and Lfe: both sides, at the same level. */
    assert(PesToneWav(PES_SPK_C, wav, size) == size);
    assert(Peak(wav + 44, frames, 0) > 0 && Peak(wav + 44, frames, 1) > 0);
    assert(PesToneWav(PES_SPK_LFE, wav, size) == size);
    assert(Peak(wav + 44, frames, 0) > 0 && Peak(wav + 44, frames, 1) > 0);

    /* Bad arguments write nothing. */
    assert(PesToneWav(-1, wav, size) == 0);
    assert(PesToneWav(PES_SPK_COUNT, wav, size) == 0);
    assert(PesToneWav(PES_SPK_L, wav, size - 1) == 0);
    assert(PesToneWav(PES_SPK_L, NULL, size) == 0);

    free(wav);
    printf("PASS: audio test tones: valid WAV per speaker, sides and level, bad arguments\n");
    return 0;
}
