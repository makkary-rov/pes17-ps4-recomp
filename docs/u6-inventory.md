<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# U-6 inventory (M1e)

Decisions for the items the M1 plan lists under U-6 and the remaining plain M1 drops.
Locations are at HEAD 12990e7 plus the M1e edits. "upstream title" names the game
whose tooling was removed; no title-specific names are used in this file.

| item | location (file:line) | plan row | decision | when |
|---|---|---|---|---|
| settings overlay | `gpu/shim/pes_overlay.cpp`; callers `gpu/shim/vk_presenter.cpp`, `gpu/shim/window.cpp`, `runtime_pad.c:312`, `gpu/shim/pesgpu.cpp:432` | U-6 (default DROP) | DROP-deferred: wired into presenter, window, pad and settings, so not a plain removal (D4) | M2, separate task |
| gbuffer heuristic | none (grep = 0) | U-6 | absent | n/a |
| memory callbacks | `gpu/shim/pesgpu.cpp:127-148` | M1 | PORT; race fix H-5 | M4 (H-5) |
| online networking switch (`PES_ONLINE`) | `src/runtime_services.c` (`runtime_services_resolve`), `gpu/pesgpu.h`, `gpu/shim/pesgpu.cpp`, `src/main.c` | H-13 remnant | dropped in M1e (Step 2) | M1e |
| mod trace (`PES_MOD_TRACE`, `PES_MODS_DIR`) | `src/runtime_file.c:236` | U-6 | KEEP (diagnostic behind a PES_* variable, off by default) | decide in ledger at M2 |
| input hooks | `gpu/shim/input/controller_backends.h` is the only remnant (H-14) | H-14 | absent (only `controller_backends.h` remains, not removed here) | n/a |
| foxgpu, G-30 shim, G-34 sound hack, G-36 `vk_globals.h` | none (grep = 0) | H-15, G-30, G-34, G-36 | absent | n/a |
| vulkan smoke test | `src/vulkan_smoke.c` (removed), `build.sh` link line, `src/main.c` dispatch | G-39 | dropped in M1e (Step 1) | M1e |
| GNM table | `gpu/shim/pes_gnm_table.inc`, `tools/re/gen_gnm_table.py`; included by `pes_gnm_hooks.cpp` | U-4 | DROP at M2 (game-derived generated table; D3). audit_publish reports it as WARN-pending | M2 |
| Windows experiments | `gpu/shim/pes_windows_experiments.cpp` | U-6 | KEEP, review at M2 | M2 |
