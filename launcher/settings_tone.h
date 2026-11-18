/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Test tones for the Audio tab (M6.5a): one short WAV per speaker of the layout (L, R, C, Lfe, Ls, Rs),
 * 16-bit stereo at 48 kHz. The output is a stereo fold-down, so the centre and LFE tones play on both
 * sides and the surround tones on their own side. Plain C with no Windows calls, tested everywhere. */
#ifndef PES_LAUNCHER_SETTINGS_TONE_H
#define PES_LAUNCHER_SETTINGS_TONE_H

#include <stddef.h>

enum PesSpeaker { PES_SPK_L = 0, PES_SPK_R = 1, PES_SPK_C = 2, PES_SPK_LFE = 3, PES_SPK_LS = 4, PES_SPK_RS = 5, PES_SPK_COUNT = 6 };

/* Bytes of one tone file (a 44-byte WAV header and the samples). */
size_t PesToneSize(void);

/* Writes the WAV file for a speaker into out. Returns the number of bytes written, or 0 if the speaker
 * is out of range or out is smaller than PesToneSize(). */
size_t PesToneWav(int speaker, unsigned char *out, size_t cap);

#endif
