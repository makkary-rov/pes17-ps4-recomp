<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Architecture

The project has three layers. Game files are supplied by the user at run time and are never part of the repository.

## Loader (`src/main.c`, `scripts/prepare.py`, `scripts/link_*`)

- `scripts/prepare.py` reads the user's dump and writes the prepared content.
- `scripts/link_*` link the PS4 modules and libc imports into the loader's image.
- `src/main.c` parses the command line, maps the executable image, and starts the guest entry point.

## Runtime (`src/runtime_*.c`, `src/win32_*.c`)

- Library services for the guest: kernel, libc, pad, file and mod access, trophies, dialogs.
- Import names are resolved through `src/import_names.inc` and the runtime tables.
- Host services (window, file system, threads) live in the `win32_*` files.

## GPU (`gpu/shadps4`, `gpu/shim`, `libpesgpu`)

- `gpu/shadps4` is the shadPS4-derived Vulkan renderer (vendored, with its own licence notes in `LICENSES/`).
- `gpu/shim` adapts the renderer to the runtime: presenter, window, pad input, settings.
- `libpesgpu` is the library the loader calls into for graphics initialisation and kernel services.

## Build

`build.sh` compiles the loader, runtime and GPU libraries with CMake for the MSYS2 CLANG64 toolchain.
See [BUILDING.md](BUILDING.md).
