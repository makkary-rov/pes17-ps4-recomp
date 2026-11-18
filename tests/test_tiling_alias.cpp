// SPDX-License-Identifier: GPL-2.0-or-later
// A GPU-drawn image read back with another guest tiling is written back to memory first
// (pes_tiling_alias.h). The upstream title draws unlicensed kits 2D-tiled (array 4, tile 14)
// and samples them 1D-tiled (array 2, tile 13); without the write-back they were black.
#include "pes_tiling_alias.h"

#include <cstdio>
#include <cstdlib>

static int failures = 0;
static void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

int main() {
    const Pes::GuestTiling kit_drawn{4, 14, false};   // colour buffer: 2D tiled thin
    const Pes::GuestTiling kit_sampled{2, 13, false}; // texture: 1D tiled thin
    check(Pes::WriteBackBeforeRetiledView(kit_sampled, kit_drawn, true),
          "GPU-drawn kit read with another tiling is written back first");
    check(!Pes::WriteBackBeforeRetiledView(kit_sampled, kit_drawn, false),
          "an image the GPU never wrote has nothing to write back");
    check(!Pes::WriteBackBeforeRetiledView(kit_drawn, kit_drawn, true),
          "the same tiling is reused, not written back");
    check(Pes::WriteBackBeforeRetiledView({4, 14, true}, kit_drawn, true),
          "alternate tiling alone is a different layout");
    // Kit bytes written as BC3 blocks through an RGBA8 storage view, then sampled as BC3.
    check(Pes::WriteBackOverlapBeforeUpload(true, true),
          "a texture over GPU-written memory gets those bytes first");
    check(!Pes::WriteBackOverlapBeforeUpload(true, false),
          "memory the GPU never wrote is already current");
    check(!Pes::WriteBackOverlapBeforeUpload(false, true),
          "a new render/storage target is written before it is read");
    if (failures) {
        return EXIT_FAILURE;
    }
    std::printf("PASS: GPU-drawn images are written back before a view with another tiling\n");
    return EXIT_SUCCESS;
}
