// SPDX-License-Identifier: GPL-2.0-or-later
// pes: a texture that reads, with another tiling, the memory a render target drew.
//
// On the console both views are the same bytes: the texture unit reads what the colour
// buffer left in memory, through its own tiling. The texture cache keeps the two as separate
// images (different guest tiling layout). Without a write-back the new image is uploaded from
// guest memory that never received the GPU's drawing: the upstream title's unlicensed kits are
// drawn 2D-tiled (array mode 4, tile mode 14) and sampled 1D-tiled (2, 13), and came out black.
// So a GPU-modified image is first stored in its own layout, then the texture reads memory.
#pragma once

namespace Pes {

struct GuestTiling {
    unsigned array_mode;
    unsigned tile_mode;
    bool alt_tile;
};

inline bool SameGuestTiling(const GuestTiling& a, const GuestTiling& b) {
    return a.array_mode == b.array_mode && a.tile_mode == b.tile_mode && a.alt_tile == b.alt_tile;
}

/// Whether the cached image must be written back to guest memory before a view with the
/// requested tiling is created over the same address from memory.
inline bool WriteBackBeforeRetiledView(const GuestTiling& requested, const GuestTiling& cached,
                                       bool cached_gpu_modified) {
    return cached_gpu_modified && !SameGuestTiling(requested, cached);
}

} // namespace Pes
