<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Status

| Milestone | State |
|---|---|
| M0 evidence baseline | done |
| M1 skeleton: import, licence audit, build repair, renames, U-6 inventory, publish guard, docs | done pending D1 (commit message, already applied) and D5 (commit of M1e by the user) |
| M2 loader parity | planned |
| M3 runtime parity | planned |
| M4 title and menu drawn | planned |
| M5 live match | planned |
| M6 kickoff | planned |
| M7 publish | planned |

## Deferred

- H-21: shadPS4 renderer delta against upstream b4e7ae7 (`docs/shadps4-delta.md`) needs a network fetch; deferred until the user approves it (D2).
- S-1 networked test: deferred with H-21 (D2). The offline clone test runs after the M1e commit.
- D3: `gpu/shim/pes_gnm_table.inc` is a game-derived generated table; dropped at M2 (U-4). The publish check reports it as a WARN until then.
- D4: the settings overlay `gpu/shim/pes_overlay.cpp` is wired into several parts of the runtime; decided in a separate task at M2.
- H-5: memory callback race in `gpu/shim/pesgpu.cpp`; fixed at M4.
