# PES 2017 PS4 recompilation (PC)

This project approaches the PS4 executable by static recompilation of the PS4 x86-64 code, run on a high-level emulation (HLE) runtime written in C, with a Vulkan renderer derived from shadPS4 for graphics.

## Disclaimer

No game files, keys or firmware are included or provided. You need your own legally obtained dump. This project is not affiliated with Konami or Sony. Trademarks belong to their owners.

## Using it

1. Build it (see BUILDING.md); the build produces `pes17launcher.exe`, `pes17settings.exe` and `pes17recomp.exe` in `out/`.
2. Run the launcher. **Install** asks for the folder of your own PS4 game dump (the one with `eboot.bin`) and remembers it; **Play** starts the game; **Settings** opens the settings window (display, keyboard, controller, audio); **Exit** closes the launcher.
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
