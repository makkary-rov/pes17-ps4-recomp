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

The launcher and settings window art is this project's own: the logo is an original wordmark, the button icons are drawn by `tools/art/make_settings_art.py`, the gamepad picture, the three tile pictures and the settings background were supplied by the project owner, and all of them are AI-generated (the launcher background is a crop of the settings background). No console maker's artwork, product photo or logo and nothing from the game's artwork is used or shipped.

## Game-derived information (decision D6)

| Allowed in this repository | Not allowed |
|---|---|
| guest addresses and lengths; SHA-256 hashes of original bytes at a site; replacement bytes we write; function addresses and our own description of what they do; import names from public sources | original game bytes (even short signatures); decompiled or disassembled listings; tables generated from the game's binary (they are produced locally by the scripts and git-ignored); game files, keys, saves |

Every game-specific patch has a record in `docs/patch-records.md`.

This document is not legal advice; a licence review is recommended.
