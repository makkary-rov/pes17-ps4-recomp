<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# 2017RECOMP: the PS4 build of PES 2017 on Windows

This project runs the PS4 build of a 2017 football game on Windows. It is not a source-code recompilation and not an emulator of the whole console: it loads the game's own x86-64 code natively and replaces the PS4 system libraries with a small high-level runtime written in C, with a Vulkan renderer derived from shadPS4 for graphics.

## Disclaimer

No game files, keys or firmware are included or provided. You need your own legally obtained dump. Pro Evolution Soccer, PES, PlayStation and related names, logos and marks belong to their owners (Konami Digital Entertainment, Sony Interactive Entertainment) and are named only to identify the game this project works with. This project is not affiliated with or endorsed by them.

## Using it

0. Easiest: download the Windows zip from the project's Releases page and unpack it anywhere. It holds the three programs, the libraries they need and a Python for the one-time preparation, so there is nothing to install or build; continue at step 2. (The zip is built by `scripts/make_release.py`, see BUILDING.md section 7.)
1. Or build it yourself (see BUILDING.md); the build produces `pes17launcher.exe`, `pes17settings.exe` and `pes17recomp.exe` in `out/`. You also need Python 3 (no extra packages) for the one-time preparation below.
2. Run the launcher. **Install** asks for the folder of your own PS4 game dump (the one with `eboot.bin`), remembers it, and prepares the game once (a few minutes; the three Python scripts in `scripts/` write their results to `game-data/` next to the launcher, with a log in `game-data/prepare.log`). **Play** starts the game (it prepares first if that has not been done); **Settings** opens the settings window (display, keyboard, controller, audio); **Exit** closes the launcher.
3. Settings are kept in `pes17recomp.ini` next to the executables. Gameplay runs at 60 or 30 frames per second; pre-match cutscenes keep the game's own 30.

## Status

See [STATUS.md](STATUS.md) for the milestone table.

## Documentation

- [BUILDING.md](BUILDING.md): build instructions (MSYS2 CLANG64)
- [ARCHITECTURE.md](ARCHITECTURE.md): loader, runtime and GPU layout
- [LEGAL.md](LEGAL.md): what the repository contains and does not contain
- [CONTRIBUTING.md](CONTRIBUTING.md): patch records and the publish check

## Licence

See LICENSE and NOTICE.
