// SPDX-License-Identifier: GPL-2.0-or-later
// Standalone CPU regression checks for the upscale trigger: no game, Vulkan device or window.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include "gpu/shadps4/video_core/renderer_vulkan/upscale_trigger.h"

using namespace Vulkan::UpscaleTrigger;

namespace {
// A PES match frame as PES_SCENE_TRACE recorded it (2026-11-23): returns the index of the
// pass the rule fires on, or -1.
struct Step {
    uint64_t scene_draw; ///< non-zero: an HDR scene draw into this image
    bool has_depth;
    uint64_t reads;  ///< image sampled
    uint64_t writes; ///< colour target
};
// Like the renderer: scene draws after the upscale still register their image.
int FirePass(SceneImages& images, const Step* steps, int count) {
    uint64_t current = 0;
    int fired = -1;
    for (int i = 0; i < count; ++i) {
        const Step& s = steps[i];
        if (s.scene_draw) {
            current = s.scene_draw;
            images.Add(current);
            continue;
        }
        if (fired < 0 && current &&
            Fires({s.has_depth, s.reads == current, images.Contains(s.writes)})) {
            fired = i;
        }
    }
    return fired;
}
} // namespace

int main() {
    constexpr uint64_t A = 0x10f9b80000, B = 0x1103000000, Post = 0x10ede00000,
                       Bloom = 0x10e68c0000, Ldr = 0x1103800000;
    const Step frame[] = {
        {A, true, 0, 0},       // 0: effects into A with the scene depth
        {0, false, A, B},      // 1: acef85d4, the A -> B copy
        {B, true, 0, 0},       // 2: transparents into B
        {0, false, B, Post},   // 3: 1ddff08b, the first post draw
        {0, false, B, Bloom},  // 4: 6ad09c92, bloom downsample
        {0, false, B, Ldr},    // 5: f74e81ed, tonemap
    };
    constexpr int count = sizeof(frame) / sizeof(frame[0]);

    SceneImages images;
    // First frame of a session: B is not known as a scene image yet, so the copy fires (one
    // pass early, once; Rasterizer::NotifySceneReader).
    assert(FirePass(images, frame, count) == 1);
    // Every later frame: after all scene draws, before the first post draw.
    assert(FirePass(images, frame, count) == 3);
    assert(FirePass(images, frame, count) == 3);

    // The rule itself.
    assert(Fires({false, true, false}));
    assert(!Fires({true, true, false}));  // scene geometry with the depth bound
    assert(!Fires({false, false, false})); // reads something else
    assert(!Fires({false, true, true}));   // writes a scene image (the A -> B copy)

    // SceneImages: null never matches; old entries leave once eight newer ones arrive.
    SceneImages ring;
    assert(!ring.Contains(0));
    ring.Add(0);
    assert(!ring.Contains(0));
    for (uint64_t i = 1; i <= 8; ++i) {
        ring.Add(i);
    }
    assert(ring.Contains(1) && ring.Contains(8));
    ring.Add(8); // already known: no slot used
    ring.Add(9);
    assert(!ring.Contains(1) && ring.Contains(2) && ring.Contains(9));

    std::puts("Upscale trigger: PASS (PES frame order, first frame, rule, scene image ring)");
    return 0;
}
