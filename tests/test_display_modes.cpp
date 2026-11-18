// SPDX-License-Identifier: GPL-2.0-or-later
// The settings screen's display list (M6.5a, user decision): 640x480 up to 1920x1080, in size order,
// each entry a real output resolution. The larger entries stay for the upscaler and overlay.
#include <cassert>
#include <cstdio>
#include "gpu/shim/pes_settings.h"

int main() {
    using namespace PesSettings;
    static_assert(DisplayModeCount == 7);
    const int expect[][2] = {{640, 480}, {800, 600}, {1024, 768}, {1280, 720}, {1366, 768}, {1600, 900}, {1920, 1080}};
    for (int i = 0; i < DisplayModeCount; ++i) {
        const int r = DisplayModeIndices[i];
        assert(r >= 0 && r < OutputCount);
        assert(OutputWidths[r] == expect[i][0] && OutputHeights[r] == expect[i][1]);
        if (i > 0) assert(OutputWidths[r] > OutputWidths[DisplayModeIndices[i - 1]]);
    }
    /* Existing saved values keep their meaning: the first ten indices are unchanged. */
    assert(OutputWidths[0] == 1280 && OutputHeights[0] == 720 && OutputWidths[1] == 1920 && OutputHeights[1] == 1080);
    std::printf("PASS: display list is 640x480 to 1920x1080 and existing output indices are unchanged\n");
    return 0;
}
