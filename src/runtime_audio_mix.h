/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Downmix of the PS4 8-channel float output to the stereo device (M6.5b).
 *
 * Input order is SDL's 7.1 order after the PS4 remap (see runtime_audio.c): FL FR FC LFE BL BR SL SR.
 * Front pair at unity gain; centre and surrounds at -3 dB (1/sqrt 2), LFE dropped (as the usual
 * ITU-R BS.775 7.1 to stereo fold-down). Sums are clamped to [-1, 1]. Doing this here, not in the
 * device API's own downmix, keeps the front level exact. */
#ifndef PES_RUNTIME_AUDIO_MIX_H
#define PES_RUNTIME_AUDIO_MIX_H

#include <stddef.h>

static inline float pes_audio_clamp1(float v) {
    return v > 1.0f ? 1.0f : v < -1.0f ? -1.0f : v;
}

/* in: frames * 8 floats (SDL 7.1 order); out: frames * 2 floats (L R). */
static inline void pes_audio_downmix_8_to_2(const float *in, float *out, size_t frames) {
    const float k = 0.70710678f;
    for (size_t f = 0; f < frames; ++f) {
        const float *s = in + f * 8;
        out[f * 2] = pes_audio_clamp1(s[0] + k * (s[2] + s[4] + s[6]));
        out[f * 2 + 1] = pes_audio_clamp1(s[1] + k * (s[2] + s[5] + s[7]));
    }
}

#endif
