/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Title patch table for CUSA04957, applied by pes_patch_apply (src/runtime_patch.h).
 * Every row has a record in docs/patch-records.md. Rows are refused (and change nothing) when the original
 * bytes at the offset do not match the recorded SHA-256. */
#ifndef PES_TITLE_CUSA04957_PATCHES_H
#define PES_TITLE_CUSA04957_PATCHES_H

#include "runtime_patch.h"

/* P-001 (workaround, root-cause task M5-R1): guest 0x156b087, 11 bytes in FUN_0256ad80 (CRI file-read
 * completion). Replaced by 11 NOPs so a failed read is logged and play continues without that asset.
 * The original bytes are not recorded here (decision D6); see docs/patch-records.md. */
static const uint8_t pes_cusa04957_p001_nops[11] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};

static const pes_patch pes_cusa04957_patches[] = {
    {"P-001", 0x156b087u, 11u, "eb3846bcba3e5f9b9c7e2d017bb9b445cb9e84fa567fe292543e0f3a064b6e11",
     pes_cusa04957_p001_nops},
};
#define PES_CUSA04957_PATCH_COUNT (sizeof(pes_cusa04957_patches) / sizeof(pes_cusa04957_patches[0]))

#endif
