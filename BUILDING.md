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
