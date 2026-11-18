<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Patch records

Each game-specific patch has one record. Records use new names only ("upstream title" for the game).
This file holds the template and the records (no title patch table is active).

| Field | Content |
|---|---|
| ID | P-xxx, ledger row ID |
| Guest address / range | e.g. 0x22d8bb3, length |
| Function | FUN_xxxxxxxx and role as understood |
| Original-bytes hash | SHA-256 of the original bytes (patch applied only if it matches; otherwise refuse and log) |
| Change | new bytes / HLE behaviour |
| Why (evidence) | dump name (private) + our summary; log line; what the PS4 would have done |
| Root cause or workaround | if workaround: the root-cause task ID |
| Test | harness scenario and pass condition |
| Status | proposed / approved (QA) / removed |

## P-001: file-read error trap (workaround)

| Field | Content |
|---|---|
| ID | P-001 |
| Guest address / range | 0x156b087, 11 bytes |
| Function | FUN_0256ad80 (Ghidra 0x256ad80): CRI file-read completion handler |
| Original-bytes hash | eb3846bcba3e5f9b9c7e2d017bb9b445cb9e84fa567fe292543e0f3a064b6e11 (the original bytes are not recorded here: decision D6) |
| Change | 11 x `90` (NOP) |
| Why (evidence) | The handler logs a file-read error when the loader status is not the expected one, then performs a deliberate invalid write. A file trace showed that the read before the fault was an entry past the end of a truncated local game archive. |
| Root cause or workaround | Workaround. The root cause was a truncated local game archive (game data, not the runtime). |
| Test | The patch engine's tests (`tests/test_title_infra.c`) cover matching, applying and refusing other bytes with synthetic data; the row-specific test was removed with the row. |
| Status | removed 2026-11-18: an intact archive was restored. Thread dumps also showed the NOP only turned the crash into a hang. |

## Patch engine and observer (src/runtime_patch.h)

`pes_patch_apply` applies an entry only if the SHA-256 of the original bytes at its offset matches the
record; otherwise the entry is refused, logged (`PES_TRACE=patch`) and changes nothing. The table is data
and starts empty: no patch is applied until its record above is approved and a test fails without it.
`pes_observer_sample` is the read-only state observer (`PES_TRACE=title`); it cannot write guest memory.
Trace subsystems: `PES_TRACE=loader,file,gpu,title,kernel,patch` (default off, read once at startup,
per-subsystem cap `PES_TRACE_CAP`, output `PES_TRACE_FILE`); `-DPES_NO_TRACE` compiles it out.

## Mapping rule (not a patch): PT_SCE_RELRO is writable

`scripts/prepare.py` `segment_flags()` maps PT_SCE_RELRO load segments read-write although their
header says read-only. No guest byte changes. Evidence: a JobGraph worker wrote into the segment at run
time and faulted while it was mapped read-only; the console therefore maps it writable. Test:
`tests/test_segment_flags.py`.
