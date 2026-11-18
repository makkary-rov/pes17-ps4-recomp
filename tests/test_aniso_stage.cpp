// SPDX-License-Identifier: GPL-2.0-or-later
// Forced anisotropy (PES_ANISO) applies only to stages that read a mipmapped image. The tone-mapping
// pass of the upstream title reads only single-level render targets (HDR scene, bloom, the 256x16
// colour-grading table); forcing anisotropy there blended table cells into coloured speckle.
#include "pes_aniso.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

static int failures = 0;
static void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

int main() {
    // Tone-mapping stage: HDR scene 1920x1080, bloom 480x270, LUT 256x16, one level each.
    check(!Pes::StageWantsForcedAniso(std::vector<unsigned>{1, 1, 1}), "single-level post-process stage");
    check(!Pes::StageWantsForcedAniso(std::vector<unsigned>{}), "stage with no images");
    // Scene stage: a mipmapped surface texture next to a single-level shadow map.
    check(Pes::StageWantsForcedAniso(std::vector<unsigned>{1, 11}), "scene stage with a mip chain");
    check(Pes::StageWantsForcedAniso(std::vector<unsigned>{2}), "two levels are a mip chain");
    if (failures) {
        return EXIT_FAILURE;
    }
    std::printf("PASS: forced anisotropy only for stages with mipmapped images\n");
    return EXIT_SUCCESS;
}
