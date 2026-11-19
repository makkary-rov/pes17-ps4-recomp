// SPDX-License-Identifier: GPL-2.0-or-later
// pes: which linear-image readbacks may be asynchronous (texture_cache.cpp, ProcessDownloadImages).
//
// A synchronous readback waits for the GPU to finish everything queued (Scheduler::Finish) before the
// image is copied into the game's memory; the profile of a match showed the game reading one small
// image pair every frame, which made the GPU finish twice a frame. An asynchronous readback queues
// the copy and writes the game's memory when the GPU gets there: the game must then tolerate seeing
// the previous value. That was only established for the 32x32, 64 bits per pixel, single-layer pair
// the profile showed (the game alternates two such images). Anything else, notably the kit staging
// textures whose contents the game uses at once, stays synchronous.

#pragma once

#include <cstdint>

namespace PesReadback {

/// True if an image of this shape may be read back asynchronously.
inline bool MayBeAsync(std::uint32_t width, std::uint32_t height, std::uint32_t depth, std::uint32_t layers,
                       std::uint32_t bits_per_pixel) {
    return width == 32 && height == 32 && depth == 1 && layers == 1 && bits_per_pixel == 64;
}

} // namespace PesReadback
