// SPDX-License-Identifier: GPL-2.0-or-later
// pes_readback_policy.h: only the 32x32, 64 bits per pixel, single-layer image shape may be read back
// asynchronously; every other shape stays synchronous (the kit staging textures in particular).
#include <cassert>
#include <cstdio>
#include "pes_readback_policy.h"

int main() {
    using PesReadback::MayBeAsync;
    assert(MayBeAsync(32, 32, 1, 1, 64));          // the per-frame pair seen in gameplay
    assert(!MayBeAsync(32, 32, 1, 1, 32));         // other pixel sizes
    assert(!MayBeAsync(32, 32, 1, 1, 128));
    assert(!MayBeAsync(16, 16, 1, 1, 64));         // smaller, larger, non-square
    assert(!MayBeAsync(64, 64, 1, 1, 64));
    assert(!MayBeAsync(32, 16, 1, 1, 64));
    assert(!MayBeAsync(128, 128, 1, 1, 32));       // a kit staging texture
    assert(!MayBeAsync(512, 512, 1, 1, 32));
    assert(!MayBeAsync(32, 32, 2, 1, 64));         // volumes and arrays
    assert(!MayBeAsync(32, 32, 1, 2, 64));
    assert(!MayBeAsync(0, 0, 0, 0, 0));
    std::puts("PASS: only the 32x32 64-bit single-layer readback may be asynchronous");
    return 0;
}
