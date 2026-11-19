<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Building

Windows, MSYS2 CLANG64. Use your own legally obtained dump; no game files are in this repository.

## 1. Install MSYS2 packages

Open the MSYS2 CLANG64 shell and run:

```
pacman -S --needed mingw-w64-clang-x86_64-clang mingw-w64-clang-x86_64-cmake mingw-w64-clang-x86_64-ninja \
  mingw-w64-clang-x86_64-pkgconf mingw-w64-clang-x86_64-vulkan-headers mingw-w64-clang-x86_64-vulkan-loader \
  mingw-w64-clang-x86_64-vulkan-memory-allocator mingw-w64-clang-x86_64-sdl3 mingw-w64-clang-x86_64-fmt \
  mingw-w64-clang-x86_64-boost mingw-w64-clang-x86_64-xxhash mingw-w64-clang-x86_64-zydis \
  mingw-w64-clang-x86_64-glslang mingw-w64-clang-x86_64-spirv-headers mingw-w64-clang-x86_64-python \
  mingw-w64-clang-x86_64-robin-map mingw-w64-clang-x86_64-ffmpeg
```

The list was derived from the build files: `find_package`, `pkg_check_modules` and `find_program` in
`gpu/CMakeLists.txt` (`tsl-robin-map` is provided by the `robin-map` package; FFmpeg by `ffmpeg`), `pkg-config`
in `build.sh`, and `python3` for the tests and scripts. `clang` pulls in `lld`, and `libc++` comes in with
the toolchain packages (cmake, ninja, glslang, python), so neither is listed. The list was verified against
the developer's installed packages (`pacman -Q`), not from a clean-machine install.
Check a package with `pacman -Q mingw-w64-clang-x86_64-<name>`.

## 2. Get the source with its submodules

```
git clone --recursive <repository-url>
```

or, in an existing checkout, `git submodule update --init`. Submodules: `gpu/third_party/imgui`,
`gpu/third_party/fsr-vulkan`, `third_party/LibAtrac9`. The `fsr-vulkan` checkout carries the patch the build expects.

Set `core.autocrlf=false` before cloning. The repository stores LF; `.gitattributes` keeps vendored CRLF files byte-identical.

### Windows clone settings

- Use `git config --global core.longpaths true` (or clone to a short path). Without it, the `fsr-vulkan` checkout fails with "Filename too long". Set it in the `fsr-vulkan` submodule too if you clone without `--global`.
- Submodules inherit `core.autocrlf` from the system or global Git config. The build tolerates CRLF in the FSR-Vulkan patch step, but `core.autocrlf=false` is advised.

## 3. Fetched dependencies (CMake FetchContent)

The build fetches three pinned dependencies unless you seed them locally:

| Name | Pin |
|---|---|
| magic_enum | v0.9.7 |
| miniz | 3.1.0 |
| xbyak | v7.24.2 |

To build without network access, point each at a local checkout with the cache variable:

```
-DFETCHCONTENT_SOURCE_DIR_MAGIC_ENUM=<path-to-magic_enum>
-DFETCHCONTENT_SOURCE_DIR_MINIZ=<path-to-miniz>
-DFETCHCONTENT_SOURCE_DIR_XBYAK=<path-to-xbyak>
```

## 4. Build and test

From the repository root, in the CLANG64 shell:

```
./build.sh          # builds out/pes17recomp.exe
./build.sh --test   # builds and runs the test programs
python3 -m unittest discover -s tests
python3 scripts/audit_publish.py
```

Diagnostics stay off unless you set a `PES_*` environment variable.

## 5. User directory (save data, trophies)

The loader writes saves and trophies to the user directory. Its value is chosen in this order:

1. `--user <dir>` on the command line.
2. The `PES_USER_DIR` environment variable.
3. The default: `<repo>/user` when the loader is in `<repo>/out` (the build output), otherwise `user` beside the loader.

The default does not depend on the working directory. The loader prints `User directory: <path>` at startup.
Save data goes to `<user dir>/savedata/1/<title id>`. The loader refuses (exit 1) a user directory that is the
game directory given with `--app0`, or lies inside it or inside its `CUSA` parent: saves never go into the game.

## 6. Programs, settings file and switches

`./build.sh` produces three programs in `out/` (Windows):

- `pes17launcher.exe`: Install (choose the game folder, saved as `game_dir`; it then prepares the game once with the Python scripts `scripts/prepare.py`, `content_profile.py` and `link_modules.py`, results in `game-data/` next to the launcher, log in `game-data/prepare.log`), Play, Settings, Exit. The one-time preparation needs Python 3 on `PATH` (no extra packages).
- `pes17settings.exe`: the settings window (Display, Keyboard, Controller, Audio, Online).
- `pes17recomp.exe`: the game loader. The launcher starts it; to start it by hand see the Play arguments in `launcher/launcher_game.c`.

All three read and write one file, `pes17recomp.ini`, next to the executables (`PES_CONFIG` overrides the path).
The programs need `SDL3.dll` and the other MSYS2 CLANG64 runtime libraries next to them or on `PATH`.

Environment switches for diagnosis (all off or default unless set):

| Variable | Effect |
|---|---|
| `PES_EARLY_FLIP_RELEASE=0` | Wait for each present before reusing a display buffer (the original, slower order). |
| `PES_SYNC_READBACK=1` | Make every GPU readback synchronous (the original path). |
| `PES_PRESENT_FRAMES=<2-8>` | Number of presentation frames (default: swapchain images + 2). |
| `PES_FRAME_STATS=1` | Print frame time, draw and stall statistics every 5 seconds. |
| `PES_DOWNLOAD_STATS=1` | Print which images the linear readback copies. |
| `PES_PROFILE=<thread names>` | Sampling profiler (Windows); stacks go to `PES_PROFILE_OUT`. Resolve with `llvm-symbolizer`. |
