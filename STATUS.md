<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Status

| Milestone | State |
|---|---|
| M0 evidence baseline | done |
| M1 skeleton: import, licence audit, build repair, renames, inventory, publish guard, docs | done |
| M2 loader parity | closed |
| M3 runtime parity | closed |
| M4 title and menu drawn | closed |
| M5 live match | closed |
| M6 kickoff | provisional pass: no post-kickoff fault in any run, two full matches played; the formal 3-of-3 run criterion is still open |
| M6.5a launcher and settings app | done, with three checks still needing the owner: starting a match from the launcher alone (the preparation chain itself is verified), a keyboard or pad rebind taking effect in a match, and the settings window's look on a high-DPI screen |
| M6.5b audio level | closed |
| M6.5c FSR 4 and DLSS, graphics sliders | moved to after the first release; FSR 3.1 (on or off, four modes) is in the release |
| M7 publish | in preparation: publish audit, history rewrite for the first commit, licence review, QA; a Windows release zip (with its own Python, nothing to install) is made by `scripts/make_release.py` |

## Deferred

- H-21: a comparison of the shadPS4-derived renderer against upstream b4e7ae7 needs a network fetch; not done, deferred by the owner (D2). No document for it exists yet.
- S-1 networked clone test: deferred with H-21 (D2).
- D3: the checked-in game-derived table `gpu/shim/pes_gnm_table.inc` was deleted in T5 (U-4). The generated table lives in the git-ignored `src/title/generated/`.
- C7b (GNM hook binding census) is N/A on the Windows build: `gpu/CMakeLists.txt` excludes `pes_gnm_hooks.cpp` (and the other Linux-only diagnostics) under WIN32.
- The in-game settings overlay `gpu/shim/pes_overlay.cpp` is wired into several parts of the runtime and is kept.

Done since: the memory callback race in `gpu/shim/pesgpu.cpp` was fixed at M4.
