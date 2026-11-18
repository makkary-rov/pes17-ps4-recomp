// SPDX-License-Identifier: GPL-2.0-or-later
// Linear images the GPU writes are read back to guest memory by default (PES_READBACK_LINEAR).
// The upstream title copies each unlicensed kit into a linear staging texture with a compute
// shader (ResolveRenderTexture -> CopyResourceToStagingTexture, guest 0x1aa6280) and reads it on
// the CPU; without the readback the CPU read zeros and the kits were black.
#include <cstdio>
#include <cstdlib>
#include "core/emulator_settings.h"

int main() {
#ifdef _WIN32
    _putenv("PES_READBACK_LINEAR=");
#else
    unsetenv("PES_READBACK_LINEAR");
#endif
    if (!EmulatorSettings.IsReadbackLinearImagesEnabled()) {
        std::printf("FAIL: linear image readback is off by default (black unlicensed kits)\n");
        return EXIT_FAILURE;
    }
    std::printf("PASS: linear image readback is on by default\n");
    return EXIT_SUCCESS;
}
