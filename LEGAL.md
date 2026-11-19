<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Legal

## In this repository

- Project code, scripts, build files and documentation, licensed as described in `LICENSE` and `NOTICE`.
- Third-party code with its licence and upstream notes: `gpu/shadps4` and other vendored code (see `LICENSES/` and `CREDITS.md`).
- Git submodule pointers for `gpu/third_party/imgui`, `gpu/third_party/fsr-vulkan` and `third_party/LibAtrac9`.

## Not in this repository

- Game files, executables, dumps, firmware, keys, saves, trophy data or any other content from the game.
- Generated tables derived from the game. They are produced locally and never committed: the generated libGnm table lives in `src/title/generated/`, which is git-ignored.
- Build outputs (`out/`) and logs.

You need your own legally obtained dump to run the project. No game files, keys or firmware are included or provided.

Pro Evolution Soccer, PES, PlayStation and related names, logos and marks belong to their owners (Konami Digital Entertainment, Sony Interactive Entertainment) and are named only to identify the game this project works with. This project is not affiliated with or endorsed by them.

The launcher can show the game's own title logo. It reads it at run time from the game folder you installed from, keeps it only on your computer, and this repository never contains it. Without a game folder it shows this project's own wordmark.

This document is not legal advice; a licence review is recommended.
