// SPDX-License-Identifier: GPL-2.0-or-later
// pes: which shader stages get the forced anisotropic filtering (PES_ANISO).
//
// Forcing anisotropy is meant for mipmapped scene textures seen at a grazing angle. A stage whose
// bound images all have a single mip level reads render targets (post processing: bloom, the
// tone-mapping colour-grading table). There the coordinates follow pixel colour, not the surface,
// and anisotropic taps along their screen-space derivative blend unrelated table cells: coloured
// speckle on high-frequency surfaces (crowd, grass, edges). Such stages keep the game's filter.
#pragma once

#include <cstdint>

namespace Pes {

/// `levels`: the mip level count of each image bound in the stage (any iterable of integers).
template <typename Levels>
bool StageWantsForcedAniso(const Levels& levels) {
    for (const auto n : levels) {
        if (n > 1) {
            return true;
        }
    }
    return false;
}

} // namespace Pes
