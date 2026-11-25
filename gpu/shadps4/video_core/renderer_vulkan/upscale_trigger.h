// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>
#include <cstdint>

// pes: where the temporal upscale runs in a PES frame (PES_SCENE_TRACE, HANDOFF-0.2). A match
// frame draws the lit scene and early effects into scene image A, copies A into scene image B
// with a full-screen draw, draws the transparents into B, then post-processes B with
// full-screen pixel-shader draws (no compute). The upscale belongs before the first post draw:
// after every scene draw, before anything reads the finished scene.
namespace Vulkan::UpscaleTrigger {

// Every image used as HDR scene colour, kept across frames: the A -> B copy writes an image
// that receives scene draws later in the same frame, which is only known from earlier frames.
class SceneImages {
public:
    void Add(uint64_t address) {
        if (!address || Contains(address)) {
            return;
        }
        addresses[next] = address;
        next = (next + 1) % addresses.size();
    }
    [[nodiscard]] bool Contains(uint64_t address) const {
        if (!address) {
            return false;
        }
        for (const uint64_t known : addresses) {
            if (known == address) {
                return true;
            }
        }
        return false;
    }

private:
    std::array<uint64_t, 8> addresses{};
    size_t next = 0;
};

struct Pass {
    bool has_depth;          ///< a depth attachment is bound (scene geometry, not a post draw)
    bool samples_scene;      ///< samples this frame's current HDR scene colour image
    bool writes_scene_image; ///< one of its colour targets has been an HDR scene target
};

// The first post-processing draw: depthless, reads the current scene colour, writes no scene
// image (that is the A -> B copy, followed by more scene draws). The caller passes only draws
// of a frame that has had an HDR scene draw and has not upscaled yet.
constexpr bool Fires(const Pass& pass) {
    return !pass.has_depth && pass.samples_scene && !pass.writes_scene_image;
}

} // namespace Vulkan::UpscaleTrigger
