// SPDX-License-Identifier: GPL-2.0-or-later
// The 8-channel to stereo fold-down keeps the front pair at unity (M6.5b): the device level of the
// port was about 8 dB under the decoded level when the device API folded it.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "runtime_audio_mix.h"

static int failures = 0;
static void expect(int ok, const char *what) {
    if (!ok) { printf("FAIL: %s\n", what); ++failures; }
}
static int near(float a, float b) { return fabsf(a - b) < 1e-5f; }

int main(void) {
    float in[8], out[2];
    /* front left only: unity on the left, nothing on the right */
    for (int i = 0; i < 8; ++i) in[i] = 0;
    in[0] = 0.5f;
    pes_audio_downmix_8_to_2(in, out, 1);
    expect(near(out[0], 0.5f) && near(out[1], 0.0f), "front left stays at unity");
    /* front right only */
    for (int i = 0; i < 8; ++i) in[i] = 0;
    in[1] = 0.25f;
    pes_audio_downmix_8_to_2(in, out, 1);
    expect(near(out[0], 0.0f) && near(out[1], 0.25f), "front right stays at unity");
    /* centre -3 dB into both sides */
    for (int i = 0; i < 8; ++i) in[i] = 0;
    in[2] = 1.0f;
    pes_audio_downmix_8_to_2(in, out, 1);
    expect(near(out[0], 0.70710678f) && near(out[1], 0.70710678f), "centre at -3 dB to both");
    /* LFE is dropped */
    for (int i = 0; i < 8; ++i) in[i] = 0;
    in[3] = 1.0f;
    pes_audio_downmix_8_to_2(in, out, 1);
    expect(near(out[0], 0.0f) && near(out[1], 0.0f), "LFE dropped");
    /* left surround (SL = index 6) to the left only, right surround (SR = index 7) to the right */
    for (int i = 0; i < 8; ++i) in[i] = 0;
    in[6] = 1.0f;
    pes_audio_downmix_8_to_2(in, out, 1);
    expect(near(out[0], 0.70710678f) && near(out[1], 0.0f), "left surround to the left at -3 dB");
    /* a loud full frame clamps instead of wrapping */
    for (int i = 0; i < 8; ++i) in[i] = 1.0f;
    pes_audio_downmix_8_to_2(in, out, 1);
    expect(out[0] == 1.0f && out[1] == 1.0f, "sums clamp to full scale");
    /* several frames are independent */
    float many_in[16], many_out[4];
    for (int i = 0; i < 16; ++i) many_in[i] = 0;
    many_in[0] = 0.1f;  /* frame 0: FL */
    many_in[8 + 1] = 0.2f; /* frame 1: FR */
    pes_audio_downmix_8_to_2(many_in, many_out, 2);
    expect(near(many_out[0], 0.1f) && near(many_out[1], 0.0f) && near(many_out[2], 0.0f) && near(many_out[3], 0.2f),
           "frames are independent");
    if (failures) return EXIT_FAILURE;
    printf("PASS: 8-channel to stereo fold keeps the front at unity\n");
    return EXIT_SUCCESS;
}
