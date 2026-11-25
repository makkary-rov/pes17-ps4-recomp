// SPDX-License-Identifier: GPL-2.0-or-later
// Standalone CPU regression checks: no game, Vulkan device or window required.
#include <array>
#include <cassert>
#include <cstdio>
#include "gpu/shadps4/video_core/renderer_vulkan/motion_history.h"

using namespace Vulkan::Motion;

int main() {
    const std::array<uint16_t, 5> indices{1000, 1007, 1002, 1007, UINT16_MAX};
    const auto range = IndexedRange(std::span(indices), -900, true);
    assert(range.first == 100 && range.count == 8);
    assert(IndexedRange(std::span(indices), -1001, true).count == 0);
    assert(IndexedRange(std::span(indices), 0, false).count == 64536);
    const std::array<uint32_t, 3> large{100000, 100003, UINT32_MAX};
    assert(IndexedRange(std::span(large), 7, true).count == 4);
    assert(IndexedRange(std::span(large), INT32_MAX, true).count == 0);
    const std::array<uint16_t, 1> restart{UINT16_MAX};
    assert(IndexedRange(std::span(restart), 0, true).count == 0);
    assert(IndexedRange(std::span<const uint16_t>{}, 0, false).count == 0);

    Draw draw{.shader = 1, .geometry = 2, .indices = 3, .topology = 4,
              .index_count = 5, .instances = 2, .first_instance = 100,
              .vertices = range};
    History history(32);
    history.NextFrame();
    const auto first = history.Prepare(draw);
    const auto duplicate = history.Prepare(draw);
    assert(first.store && !first.load && first.first_vertex == 100);
    assert(first.first_instance == 100 && first.instances == 2);
    assert(duplicate.store == first.store + 16); // shared geometry, different draw
    assert(history.Used() == 32); // firstInstance must NOT multiply the allocation
    assert(!history.Prepare(draw).store && history.stats.exhausted == 1);
    history.NextFrame();
    const auto next = history.Prepare(draw);
    assert(next.load == first.store && next.store != first.store);
    assert(history.Prepare(draw).load == duplicate.store);
    history.NextFrame(); // a missing frame invalidates the match
    history.NextFrame();
    assert(!history.Prepare(draw).load);

    // A changed index buffer may reference formerly unused holes inside the same min/max.
    // Do not accept that history merely because its addresses/counts stayed the same.
    history.NextFrame();
    auto changed = draw;
    changed.topology++;
    assert(!history.Prepare(changed).load);
    changed = draw;
    changed.first_instance++;
    assert(!history.Prepare(changed).load);
    history.NextFrame();
    changed = draw;
    changed.vertices.first++;
    assert(!history.Prepare(changed).load);

    History overflow(32);
    draw.vertices.first = 0;
    draw.vertices.count = INT32_MAX;
    draw.instances = UINT32_MAX;
    draw.first_instance = 0;
    assert(!overflow.Prepare(draw).store);
    assert(overflow.stats.exhausted == 1);
    draw.first_instance = 10;
    assert(!overflow.Prepare(draw).store && overflow.stats.invalid == 1);
    draw.first_instance = 0;
    draw.vertices.first = INT32_MAX;
    draw.vertices.count = 2;
    assert(!overflow.Prepare(draw).store && overflow.stats.invalid == 2);

    // Small skeletons: history only while the bone palette changes (weapon spinning).
    History gated(1024);
    const History::GateKey weapon{.shader = 7, .index_count = 3, .instances = 1};
    const auto frame = [&](uint64_t palette) {
        gated.NextFrame();
        return gated.Moving(weapon, palette);
    };
    assert(!gated.Moving(weapon, 1) && gated.stats.still == 1); // first sight
    assert(!frame(1));                    // same bones: camera motion
    assert(frame(2) && frame(3));         // spinning
    assert(!frame(3));                    // stopped
    gated.NextFrame();
    gated.NextFrame();
    assert(!gated.Moving(weapon, 4));     // after a skipped frame: no comparison
    assert(!gated.Moving(weapon, 5));     // second occurrence of the same draw
    assert(ClassifyBuffer(864) == BufferRole::Other && ClassifyBuffer(416) == BufferRole::Other);
    assert(ClassifyBuffer(96) == BufferRole::SmallSkeleton &&
           ClassifyBuffer(384) == BufferRole::SmallSkeleton);
    assert(ClassifyBuffer(656) == BufferRole::Skeleton && ClassifyBuffer(64) == BufferRole::Other);
    // PES: camera constants never gate a draw; object constants do; player skeletons always.
    assert(ClassifyBuffer(480) == BufferRole::Other);
    assert(ClassifyBuffer(272) == BufferRole::SmallSkeleton);
    assert(ClassifyBuffer(3072) == BufferRole::Skeleton);

    // Index ranges: reused between full scans, rescanned every Revalidate frames, trimmed.
    IndexRangeCache ranges;
    uint32_t scans = 0;
    const IndexRangeCache::Key mesh{.address = 0x1000, .count = 3, .index_size = 2};
    const auto scan = [&] {
        ++scans;
        return IndexRangeCache::Result{{4, 3}, 99};
    };
    assert(ranges.Get(mesh, 0, scan).topology == 99 && scans == 1);
    assert(ranges.Get(mesh, IndexRangeCache::Revalidate - 1, scan).range == (VertexRange{4, 3}));
    assert(scans == 1);
    ranges.Get(mesh, IndexRangeCache::Revalidate, scan);
    assert(scans == 2);
    ranges.Trim(IndexRangeCache::Revalidate + IndexRangeCache::Unused + 1);
    assert(ranges.Size() == 0);

    assert(JitterPhases(1920, 1920) == 8);
    assert(JitterPhases(1280, 1920) == 18);
    assert(JitterPhases(960, 1920) == 32);
    assert(JitterPhases(640, 1920) == 72);
    assert(JitterPhases(0, 1920) == 8);
    static_assert(!JitterAllowed(true, false), "no resolve last frame: no jitter");
    static_assert(!JitterAllowed(false, true));
    static_assert(JitterAllowed(true, true));
    {
        const auto close = [](float a, float b) { return std::abs(a - b) <= 1e-3f * std::abs(b); };
        // PES pre-match camera (zs -0.0001, zo 0.1): near 0.1, far 1000.
        const auto menu = PlanesFromProjection(-0.0001f, 0.100010f, true);
        assert(close(menu.near_plane, 0.1f) && close(menu.far_plane, 1000.1f));
        // PES broadcast camera (zs -0.010819, zo 43.277344): near about 42.8, far about 4000.
        const auto match = PlanesFromProjection(-0.010819f, 43.277344f, true);
        assert(close(match.near_plane, 42.81f) && close(match.far_plane, 4000.1f));
        assert(match.near_plane > 0.0f && match.far_plane > match.near_plane);
        // Reversed infinite (zs 0): far is the 1e6 stand-in.
        assert(PlanesFromProjection(0.0f, 0.1f, true).far_plane == 1.0e6f);
        // Standard Z with near 0.5, far 100: zs = -zo / n and zs + zo / f = 1.
        const float zo = 1.0f / (1.0f / 100.0f - 1.0f / 0.5f), zs = -zo / 0.5f;
        const auto standard = PlanesFromProjection(zs, zo, false);
        assert(close(standard.near_plane, 0.5f) && close(standard.far_plane, 100.0f));
    }
    {
        // A mesh drawn twice per frame (two LED panels) must not borrow the other's history
        // when unique_only is set; a single draw keeps its history; skeletons opt out.
        History panels(1024);
        Draw panel{.shader = 9, .geometry = 10, .indices = 11, .topology = 1, .index_count = 6,
                   .instances = 1, .first_instance = 0, .vertices = {0, 4}};
        Draw ball = panel;
        ball.geometry = 20;
        panels.NextFrame();
        panels.Prepare(panel, true);
        panels.Prepare(panel, true);
        panels.Prepare(ball, true);
        panels.NextFrame();
        assert(!panels.Prepare(panel, true).load);  // repeated last frame
        assert(!panels.Prepare(panel, true).load);  // second occurrence
        assert(panels.stats.repeated == 2);
        assert(panels.Prepare(ball, true).load);     // unique: history
        panels.NextFrame();
        assert(panels.Prepare(panel, false).load);   // skeleton: history as before
    }
    std::puts("Motion history: PASS (ranges, restart, offsets, matching, capacity, gating, index cache, jitter, repeats)");
}
