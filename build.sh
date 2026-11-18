#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail
cd -- "$(dirname -- "$0")"
mkdir -p out
if [[ -z ${CC:-} ]]; then
    CC=$(command -v cc || command -v gcc || true)
    if [[ -z $CC ]]; then
        for candidate in /nix/store/*-gcc-wrapper-*/bin/gcc; do
            if [[ -x $candidate ]]; then CC=$candidate; break; fi
        done
    fi
fi
if [[ -z ${CC:-} ]]; then echo 'Install GCC/Clang or set CC.' >&2; exit 1; fi
# Dependencies come from pkg-config (Vulkan loader/headers, SDL3).
if ! { command -v pkg-config >/dev/null && pkg-config --exists vulkan sdl3 && command -v cmake >/dev/null && command -v ninja >/dev/null; }; then
    echo 'Need pkg-config with vulkan and sdl3, cmake and ninja (MSYS2 CLANG64 packages).' >&2; exit 1
fi
read -r -a includes <<< "$(pkg-config --cflags vulkan sdl3)"
read -r -a libraries <<< "$(pkg-config --libs vulkan sdl3)"
# Windows: MSYS2 CLANG64 shell (README "Windows": clang, libc++ and lld, whose threads and locks
# are the Win32 ones). The GPU library is static there (a DLL cannot leave the runtime_* symbols
# to the executable), the runtime adds its Win32 files (src/win32_*.c), runs without pthreads,
# and the executable keeps a console and bottom-up addresses (guest-visible host memory must
# stay below 1 TiB).
windows=
case $(uname -s) in MINGW*|MSYS*|CYGWIN*) windows=1 ;; esac
cstd=(-std=c11)
threads=(-pthread)
if [[ -n $windows ]]; then
    threads=()
    libraries=("${libraries[@]/-mwindows/-mconsole}")
    cstd=(-std=gnu11 -D_FILE_OFFSET_BITS=64 -D_WIN32_WINNT=0x0A00)
fi
# GPU library (shadPS4 video core + drivers), built by CMake into out/gpu/libpesgpu.so.
# PES_PGO: generate (instrumented build that writes pgo/ while the game runs), use, off.
# Default: off. PES_LTO=OFF disables link-time optimization.
pgo=${PES_PGO:-off}
if [[ $pgo == generate ]]; then mkdir -p pgo; fi
# Submodules (git clone --recursive, or: git submodule update --init) and this port's changes
# to FSR-Vulkan (gpu/patches/fsr-vulkan), applied to its working tree once.
if [[ ! -f gpu/third_party/fsr-vulkan/CMakeLists.txt || ! -f gpu/third_party/imgui/imgui.h ]]; then
    git submodule update --init --recursive
fi
for patch in gpu/patches/fsr-vulkan/*.patch; do
    if ! git -C gpu/third_party/fsr-vulkan apply --reverse --check --ignore-whitespace "$PWD/$patch" 2>/dev/null; then
        git -C gpu/third_party/fsr-vulkan apply --ignore-whitespace "$PWD/$patch"
    fi
done
cmake -S gpu -B out/gpu -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DPES_PGO="$pgo" \
    -DPES_LTO="${PES_LTO:-ON}" -DPES_PGO_DIR="$PWD/pgo" >/dev/null
echo "GPU library: PGO $pgo, LTO ${PES_LTO:-ON}"
# A failed GPU build must stop here: an older libpesgpu.so would otherwise be used silently.
if ! ninja -C out/gpu pesgpu > out/gpu-build.log 2>&1; then
    grep -v '^\[' out/gpu-build.log | tail -40 >&2
    echo 'GPU library build failed (full log: out/gpu-build.log)' >&2; exit 1
fi
# $ORIGIN/gpu: packaged copies keep the library next to the binary without patching it.
gpu=(-Lout/gpu -lpesgpu -Wl,-rpath,'$ORIGIN/gpu' -Wl,-rpath,"$PWD/out/gpu" -rdynamic)
runtime=(src/runtime*.c)
link=(-no-pie)
if [[ -n $windows ]]; then
    # out/gpu/pesgpu_link.txt (gpu/CMakeLists.txt): libpesgpu.a and its dependencies, one per line.
    mapfile -t gpu < out/gpu/pesgpu_link.txt
    runtime+=(src/win32_*.c)
    # The static library's link-time optimization (ThinLTO in lld) runs here; on Linux it runs in
    # the .so link.
    link=(-Wl,--disable-dynamicbase,--disable-high-entropy-va -lwinmm -lws2_32 -lpsapi -flto=thin)
fi
# Third-party decoders: compiled once, without this project's -Werror policy.
atrac9=(third_party/LibAtrac9/C/src/*.c)
if [[ ! -f out/libatrac9.a || -n $(find third_party/LibAtrac9/C/src -newer out/libatrac9.a -name '*.c') ]]; then
    rm -rf out/atrac9 && mkdir -p out/atrac9
    for source in "${atrac9[@]}"; do "$CC" -std=c99 -O2 -g -w -c "$source" -o "out/atrac9/$(basename "${source%.c}").o"; done
    ar rcs out/libatrac9.a out/atrac9/*.o
fi
# Windows: the link includes the GPU library's link-time optimization (a minute or more), and
# run.bat builds before every start, so an up-to-date executable is kept.
gpu_inputs=(out/gpu/libpesgpu.a out/gpu/pesgpu_link.txt)
if [[ -n $windows ]]; then
    # Native bridge dependencies can change without changing libpesgpu.a itself.
    for dependency in "${gpu[@]}"; do
        [[ ! -f $dependency ]] || gpu_inputs+=("$dependency")
    done
fi
if [[ -n $windows && -f out/pes17recomp.exe &&
      -z $(find src gpu/pesgpu.h "${gpu_inputs[@]}" out/libatrac9.a build.sh \
               -newer out/pes17recomp.exe -print -quit) ]]; then
    echo "Up to date: $PWD/out/pes17recomp.exe"
else
"$CC" "${cstd[@]}" -O2 -g -Wall -Wextra -Werror "${threads[@]}" "${includes[@]}" -I. -Isrc src/main.c "${runtime[@]}" out/libatrac9.a -lm "${gpu[@]}" "${libraries[@]}" "${link[@]}" -o out/pes17recomp
echo "Built $PWD/out/pes17recomp"
fi
if [[ ${1:-} == --test ]]; then
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread "${includes[@]}" -I. -Isrc tests/test_pad.c src/runtime_trace.c "${libraries[@]}" -o out/pad-test
    out/pad-test
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -I. -Isrc tests/test_runtime.c "${runtime[@]}" out/libatrac9.a -lm "${gpu[@]}" "${libraries[@]}" ${windows:+-Wl,--disable-dynamicbase,--disable-high-entropy-va} -o out/runtime-test
    out/runtime-test
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -Isrc tests/test_file_mods.c ${windows:+-lws2_32} -o out/file-mods-test
    out/file-mods-test
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -I. -Isrc tests/test_sema.c "${runtime[@]}" out/libatrac9.a -lm "${gpu[@]}" "${libraries[@]}" -o out/sema-test
    out/sema-test
    "$CC" -std=c11 -D_GNU_SOURCE -O2 -g -Wall -Wextra -Werror -I. -Isrc tests/test_content.c src/runtime_content.c -o out/content-test
    out/content-test
    "$CC" -std=c11 -D_GNU_SOURCE -O2 -g -Wall -Wextra -Werror -pthread -Isrc tests/test_title_infra.c src/runtime_trace.c src/runtime_patch.c -o out/title-infra-test
    out/title-infra-test
    "$CC" -std=c11 -D_GNU_SOURCE -DPES_NO_TRACE -O2 -g -Wall -Wextra -Werror -pthread -Isrc tests/test_title_infra.c src/runtime_trace.c src/runtime_patch.c -o out/title-infra-notrace-test
    out/title-infra-notrace-test
    "${CXX:-clang++}" -std=c++20 -O2 -Wall -Wextra -Werror -Igpu/shim tests/test_aniso_stage.cpp -o out/aniso-stage-test
    out/aniso-stage-test
    "$CC" -std=c11 -O2 -Wall -Wextra -Werror -Isrc tests/test_audio_mix.c -o out/audio-mix-test -lm
    out/audio-mix-test
    "$CC" -std=c11 -O2 -Wall -Wextra -Werror -Wno-unused-parameter -Ilauncher tests/test_settings_model.c launcher/settings_model.c -o out/settings-model-test
    out/settings-model-test
    "$CC" -std=c11 -O2 -Wall -Wextra -Werror -Ilauncher tests/test_settings_keys_model.c launcher/settings_keys.c -o out/settings-keys-model-test
    out/settings-keys-model-test
    "${CXX:-clang++}" -std=c++20 -O2 -UNDEBUG -Wall -Wextra -Werror -I. -Igpu -Igpu/shim tests/test_display_modes.cpp gpu/shim/pes_settings.cpp -o out/display-modes-test
    (cd out && ./display-modes-test)
    "${CXX:-clang++}" -std=c++20 -O2 -UNDEBUG -Wall -Wextra -Werror -I. -Igpu -Igpu/shim tests/test_settings_keys.cpp gpu/shim/pes_settings.cpp -o out/settings-keys-test
    (cd out && ./settings-keys-test)
    if [[ -n $windows ]]; then
        # The settings window shell (launcher/): Windows only, with its visual-style manifest.
        windres --input launcher/pes17settings.rc --output out/pes17settings.res.o -O coff
        "$CC" -municode -O2 -Wall -Wextra -Werror -mwindows launcher/settings_window.c launcher/settings_display.c launcher/settings_keyboard.c launcher/settings_keys.c launcher/settings_model.c out/pes17settings.res.o -o out/pes17settings.exe -lcomctl32 -luser32 -lgdi32 -lkernel32
    fi
    "${CXX:-clang++}" -std=c++20 -O2 -UNDEBUG -Wall -Wextra -Werror -I. -Igpu -Igpu/shim tests/test_settings.cpp gpu/shim/pes_settings.cpp -o out/settings-test
    (cd out && ./settings-test)
    if [[ -n $windows ]]; then
        "$CC" -std=c11 -O2 -Wall -Wextra -Werror -Isrc tests/test_minidump.c src/win32_crash.c -o out/minidump-test
        out/minidump-test out/minidump-test.dmp
    fi
    "${CXX:-clang++}" -std=c++20 -O2 -Wall -Wextra -Werror -Igpu/shim -Igpu/shadps4 tests/test_readback_default.cpp -o out/readback-default-test
    out/readback-default-test
fi
