# SPDX-License-Identifier: GPL-2.0-or-later
# pes GPU library: shadPS4 video core + shader recompiler + Gnm/VideoOut
# drivers, adapted to the native_probe runtime through shims in shim/.
cmake_minimum_required(VERSION 3.24)
project(pesgpu LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()
# Frame pointers keep the loader's thread dumps (watchdog, SIGUSR2 sampling) walkable.
add_compile_options(-fno-omit-frame-pointer)

# pes: link-time optimization (PES_LTO, default on) and profile-guided optimization
# (PES_PGO: off, generate = instrumented build writing into PES_PGO_DIR, use = build with that
# profile). Covers pesgpu and the static libraries linked into it. No -march: the same build
# runs on the Steam Deck.
option(PES_LTO "Link-time optimization of pesgpu" ON)
set(PES_PGO "off" CACHE STRING "Profile-guided optimization: off, generate, use")
set(PES_PGO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../pgo" CACHE PATH "Profile directory")
if (PES_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT pes_ipo_ok OUTPUT pes_ipo_error)
    if (pes_ipo_ok)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
    else()
        message(WARNING "LTO unavailable: ${pes_ipo_error}")
    endif()
endif()
if (PES_PGO STREQUAL "generate")
    # Atomic counters: the GPU, recording, preparation and copy threads run the same code.
    add_compile_options(-fprofile-generate=${PES_PGO_DIR} -fprofile-update=atomic)
    add_link_options(-fprofile-generate=${PES_PGO_DIR})
    add_compile_definitions(PES_PGO_GENERATE)
elseif (PES_PGO STREQUAL "use")
    # Partial training keeps unprofiled code optimized normally; counters from concurrent
    # threads can be slightly inconsistent (correction). Functions changed since the profile
    # (or built differently for it, pesgpu_init) are compiled without it.
    add_compile_options(-fprofile-use=${PES_PGO_DIR} -fprofile-partial-training
                        -fprofile-correction -Wno-missing-profile -Wno-coverage-mismatch)
endif()

find_package(Vulkan REQUIRED)
find_package(fmt REQUIRED)
find_package(Boost REQUIRED)
find_package(tsl-robin-map REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(XXHASH REQUIRED IMPORTED_TARGET libxxhash)
pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)
find_package(Zydis REQUIRED)
if (WIN32)
    # pes: Windows (MSYS2 MINGW64). pesgpu is a static library linked into pes17recomp.exe
    # (a DLL cannot leave the runtime_* symbols undefined). Packages MSYS2 does not ship
    # (magic_enum, miniz, xbyak) are fetched; VMA is header-only there.
    include(FetchContent)
    find_package(magic_enum QUIET)
    if (NOT TARGET magic_enum::magic_enum)
        FetchContent_Declare(magic_enum GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
            GIT_TAG v0.9.7 GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(magic_enum)
    endif()
    find_package(miniz QUIET)
    if (NOT TARGET miniz::miniz)
        set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
        set(BUILD_NO_STDIO OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(miniz GIT_REPOSITORY https://github.com/richgel999/miniz.git
            GIT_TAG 3.1.0 GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(miniz)
        if (NOT TARGET miniz::miniz)
            add_library(miniz::miniz ALIAS miniz)
        endif()
    endif()
    find_package(xbyak QUIET)
    if (NOT TARGET xbyak::xbyak)
        FetchContent_Declare(xbyak GIT_REPOSITORY https://github.com/herumi/xbyak.git
            GIT_TAG v7.24.2 GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(xbyak)
    endif()
    find_package(VulkanMemoryAllocator CONFIG QUIET)
    if (NOT TARGET GPUOpen::VulkanMemoryAllocator)
        find_path(VMA_INCLUDE_DIR vk_mem_alloc.h PATH_SUFFIXES vma REQUIRED)
        add_library(GPUOpen::VulkanMemoryAllocator INTERFACE IMPORTED)
        set_target_properties(GPUOpen::VulkanMemoryAllocator PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES ${VMA_INCLUDE_DIR})
    endif()
else()
    find_package(magic_enum REQUIRED)
    find_package(VulkanMemoryAllocator CONFIG REQUIRED)
    pkg_check_modules(X11 REQUIRED IMPORTED_TARGET x11)
    find_package(miniz REQUIRED)
endif()
find_program(GLSLANG_COMPILER NAMES glslang glslangValidator REQUIRED)

set(SIRIT_USE_SYSTEM_SPIRV_HEADERS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/sirit EXCLUDE_FROM_ALL)

# pes: FSR 3.1 temporal upscaling / frame generation, native Vulkan (MIT, FireBurn/FSR-Vulkan).
set(FFX_VK_PORTABLE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FFX_VK_PORTABLE_BUILD_PROBE OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/fsr-vulkan EXCLUDE_FROM_ALL)
if (WIN32)
    # AMD's FidelityFX sources rely on MSVC's transitive includes (memset, floor, placement new,
    # swprintf_s) on _WIN32.
    foreach(ffx_target IN ITEMS ffx_vulkan_portable ffx_vulkan_fsr3_vk_backend_1_1_4
            ffx_vulkan_fsr3_host_1_1_4 ffx_vulkan_fsr4_v07_vulkan ffx_vulkan_fsr4_v07_assets)
        if (TARGET ${ffx_target})
            target_compile_options(${ffx_target} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include cstdio>"
                "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include cwchar>" "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include cstring>"
                "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include cmath>" "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include new>"
                "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include mutex>"
                "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include \"${CMAKE_CURRENT_SOURCE_DIR}/shim/ffx_windows_compat.h\">")
        endif()
    endforeach()
endif()

# Host shaders -> headers (same scheme as shadPS4's host_shaders/CMakeLists.txt).
set(SHADER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/shadps4/video_core/host_shaders)
set(SHADER_INCLUDE ${CMAKE_CURRENT_BINARY_DIR}/include)
set(SHADER_DIR ${SHADER_INCLUDE}/video_core/host_shaders)
set(SHADER_HEADERS "")
function(add_host_shader FILENAME SHADER_NAME)
    set(OUT ${SHADER_DIR}/${SHADER_NAME}.h)
    string(TOUPPER ${SHADER_NAME} SYMBOL)
    set(DEFINES ${ARGN})
    list(TRANSFORM DEFINES PREPEND "-D")
    add_custom_command(OUTPUT ${OUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_DIR}
        COMMAND ${GLSLANG_COMPILER} --target-env vulkan1.3 -o ${OUT} --vn ${SYMBOL} ${DEFINES} ${SHADER_SRC}/${FILENAME}
        COMMAND ${CMAKE_COMMAND} "-DHEADER_FILE=${OUT}" -P ${SHADER_SRC}/AddShaderHeaderInclude.cmake
        DEPENDS ${SHADER_SRC}/${FILENAME} VERBATIM)
    set(SHADER_HEADERS ${SHADER_HEADERS} ${OUT} PARENT_SCOPE)
endfunction()
add_host_shader(color_to_ms_depth.frag color_to_ms_depth_frag)
add_host_shader(ms_image_blit.frag ms_image_blit_frag)
add_host_shader(ms_image_blit.frag ms_image_blit_msaa_frag SRC_MSAA=1)
add_host_shader(fault_buffer_process.comp fault_buffer_process_comp)
add_host_shader(fs_tri.vert fs_tri_vert)
add_host_shader(fsr.comp fsr_easu_comp SAMPLE_EASU=1)
add_host_shader(fsr.comp fsr_rcas_comp SAMPLE_RCAS=1)
add_host_shader(post_process.frag post_process_frag)
add_host_shader(camera_motion.comp camera_motion_comp)
add_host_shader(upscale_merge.comp upscale_merge_comp)
add_host_shader(upscale_reactive.comp upscale_reactive_comp)
add_host_shader(taa.comp taa_comp)
add_host_shader(taa_sharpen.comp taa_sharpen_comp)
add_host_shader(taa_sharpen.comp taa_sharpen_ldr_comp LDR_OUTPUT=1)
add_host_shader(fsr4_color.comp fsr4_decode_comp)
add_host_shader(fsr4_color.comp fsr4_encode_comp ENCODE=1)
add_host_shader(fsr4_reactive.comp fsr4_reactive_comp)
add_host_shader(buffer_multi_copy.comp buffer_multi_copy_comp)
add_host_shader(depth_resample.frag depth_resample_frag)
add_host_shader(depth_resample.frag depth_stencil_resample_frag HAS_STENCIL=1)
add_host_shader(depth_resample.frag depth_stencil_bits_frag STENCIL_BITS=1)
add_host_shader(camera_motion.comp camera_motion_debug_comp DEBUG_OVERLAY=1)
foreach(BPP IN ITEMS 8 16 32 64 96 128)
    add_host_shader(tiling.comp tiling_micro_${BPP}_comp BITS_PER_PIXEL=${BPP} IS_MACRO_TILED=0)
    add_host_shader(tiling.comp tiling_macro_${BPP}_comp BITS_PER_PIXEL=${BPP} IS_MACRO_TILED=1)
endforeach()
add_custom_target(host_shaders DEPENDS ${SHADER_HEADERS})

file(GLOB_RECURSE VIDEO_CORE CONFIGURE_DEPENDS shadps4/video_core/*.cpp)
file(GLOB_RECURSE RECOMPILER CONFIGURE_DEPENDS shadps4/shader_recompiler/*.cpp)
list(FILTER VIDEO_CORE EXCLUDE REGEX "renderdoc\\.cpp$")
set(COMMON
    shadps4/common/decoder.cpp shadps4/common/error.cpp
    shadps4/common/io_file.cpp shadps4/common/native_clock.cpp shadps4/common/number_utils.cpp
    shadps4/common/path_util.cpp shadps4/common/rdtsc.cpp shadps4/common/spin_lock.cpp shadps4/common/recursive_lock.cpp
    shadps4/common/string_util.cpp shadps4/common/thread.cpp shadps4/common/signal_context.cpp
    shim/common/logging/log.cpp)
file(GLOB AVPLAYER CONFIGURE_DEPENDS shadps4/core/libraries/avplayer/*.cpp)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswscale libswresample)
if(WIN32)
    # Relocated MSYS2 pkg-config can return bin/../include. Clang does not
    # recognize that spelling as its implicit C include directory and places it
    # ahead of libc++ wrappers, breaking <cctype>/<cwchar>. Canonicalize only
    # existing imported include paths; never reorder the C++ standard library.
    foreach(pes_pkg IN ITEMS PkgConfig::XXHASH PkgConfig::SDL3 PkgConfig::FFMPEG)
        get_target_property(pes_includes ${pes_pkg} INTERFACE_INCLUDE_DIRECTORIES)
        set(pes_normalized)
        foreach(pes_include IN LISTS pes_includes)
            if(IS_DIRECTORY "${pes_include}")
                file(REAL_PATH "${pes_include}" pes_include)
            endif()
            list(APPEND pes_normalized "${pes_include}")
        endforeach()
        set_target_properties(${pes_pkg} PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${pes_normalized}")
    endforeach()
endif()
file(GLOB SHIM_SOURCES CONFIGURE_DEPENDS shim/*.cpp shim/*/*.cpp)
if (WIN32)
    # These optional experiments use Linux dma-buf mappings and SIGTRAP hooks. Keep the
    # section-backed Win32 memory and its vectored crash diagnostics; do not emulate dma-buf.
    list(FILTER SHIM_SOURCES EXCLUDE REGEX "/pes_(guest_memory|gnm_hooks|heap_sites)\\.cpp$")
    set(PESGPU_LIBRARY_TYPE STATIC)
else()
    set(PESGPU_LIBRARY_TYPE SHARED)
endif()
add_library(pesgpu ${PESGPU_LIBRARY_TYPE} ${VIDEO_CORE} ${RECOMPILER} ${COMMON} ${SHIM_SOURCES}
    shadps4/core/libraries/gnmdriver/gnmdriver.cpp
    shadps4/core/libraries/videoout/driver.cpp shadps4/core/libraries/videoout/video_out.cpp
    shadps4/core/libraries/kernel/equeue.cpp ${AVPLAYER}
    shadps4/core/libraries/videodec/video_utils.cpp)
add_dependencies(pesgpu host_shaders)
# In-game settings menu: Dear ImGui (third_party/imgui, MIT) with its Vulkan backend, and an
# embedded Cyrillic font (third_party/fonts).
target_sources(pesgpu PRIVATE third_party/imgui/imgui.cpp third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_vulkan.cpp)
target_include_directories(pesgpu PRIVATE third_party/imgui third_party/imgui/backends)
target_compile_definitions(pesgpu PRIVATE IMGUI_IMPL_VULKAN_NO_PROTOTYPES
    PES_FONT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/third_party/fonts/DejaVuSans.ttf")
set_source_files_properties(shim/pes_overlay.cpp PROPERTIES
    OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/fonts/DejaVuSans.ttf)
target_include_directories(pesgpu PRIVATE shim shadps4 ${SHADER_INCLUDE} third_party/gcn/include third_party/half/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/title/generated)
target_compile_definitions(pesgpu PRIVATE BOOST_ASIO_STANDALONE NDEBUG)
target_compile_options(pesgpu PRIVATE -Wno-deprecated-declarations -Wno-missing-field-initializers)
# The Linux shared library is loaded at process startup; Windows links it statically.
if(NOT WIN32)
    target_compile_options(pesgpu PRIVATE -ftls-model=initial-exec)
    target_link_options(pesgpu PRIVATE -Wl,--allow-shlib-undefined)
endif()
target_link_libraries(pesgpu PRIVATE ffx-vulkan::portable ffx-vulkan::fsr4-v07-vulkan)
target_link_libraries(pesgpu PUBLIC Vulkan::Headers fmt::fmt Boost::headers magic_enum::magic_enum
    tsl::robin_map GPUOpen::VulkanMemoryAllocator PkgConfig::XXHASH PkgConfig::SDL3 PkgConfig::FFMPEG miniz::miniz Zydis::Zydis sirit)
if (WIN32)
    # Windows headers: no min/max macros, no winsock 1 clash; Windows 10+ APIs (SetThreadDescription).
    target_compile_definitions(pesgpu PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)
    # IOFile's Windows mappings require OneCore, including standalone renderer tests.
    target_link_libraries(pesgpu PUBLIC xbyak::xbyak onecore)
    # pes17recomp.exe links libpesgpu.a with these (out/gpu/pesgpu_link.txt, one argument per line,
    # in link order): the static library's transitive dependencies.
    include(cmake/LinkClosure.cmake)
    pes_write_link_closure(pesgpu ${CMAKE_CURRENT_BINARY_DIR}/pesgpu_link.txt)
else()
    # Runtime symbols (runtime_memory_*, runtime_process_*) resolve against pes17recomp.
    target_link_options(pesgpu PRIVATE -Wl,--allow-shlib-undefined)
    target_link_libraries(pesgpu PUBLIC PkgConfig::X11)
endif()

# Explicit test targets; neither launches the game nor needs a Vulkan device.
add_executable(pipeline-cache-blob-test EXCLUDE_FROM_ALL ../tests/test_pipeline_cache_blob.cpp)
target_compile_options(pipeline-cache-blob-test PRIVATE -UNDEBUG -Wall -Wextra -Werror)
target_include_directories(pipeline-cache-blob-test PRIVATE ..)
add_executable(image-overlap-test EXCLUDE_FROM_ALL ../tests/test_image_overlap.cpp ../tests/test_gpu_runtime_stubs.cpp)
target_compile_options(image-overlap-test PRIVATE -UNDEBUG)
target_include_directories(image-overlap-test PRIVATE shim shadps4 third_party/gcn/include)
target_link_libraries(image-overlap-test PRIVATE pesgpu)
if (WIN32)
    target_link_libraries(image-overlap-test PRIVATE winmm ws2_32 psapi onecore)
endif()
set_source_files_properties(../tests/test_image_overlap.cpp PROPERTIES
    COMPILE_OPTIONS "-fvisibility=hidden;-fvisibility-inlines-hidden")
add_executable(pipeline-archive-test EXCLUDE_FROM_ALL ../tests/test_pipeline_archive.cpp)
target_compile_options(pipeline-archive-test PRIVATE -UNDEBUG)
target_include_directories(pipeline-archive-test PRIVATE shadps4)
target_link_libraries(pipeline-archive-test PRIVATE fmt::fmt)
add_executable(motion-history-test EXCLUDE_FROM_ALL ../tests/test_motion_history.cpp)
target_compile_options(motion-history-test PRIVATE -UNDEBUG -Wall -Wextra -Werror)
target_include_directories(motion-history-test PRIVATE ..)
add_executable(motion-shader-test EXCLUDE_FROM_ALL ../tests/test_motion_shaders.cpp ../tests/test_gpu_runtime_stubs.cpp)
target_compile_options(motion-shader-test PRIVATE -UNDEBUG)
target_include_directories(motion-shader-test PRIVATE shim shadps4 third_party/gcn/include)
target_link_libraries(motion-shader-test PRIVATE pesgpu)
if (WIN32)
    target_link_libraries(motion-shader-test PRIVATE winmm ws2_32 psapi onecore)
endif()
add_executable(ui-composition-test EXCLUDE_FROM_ALL ../tests/test_ui_composition.cpp)
target_compile_options(ui-composition-test PRIVATE -UNDEBUG -Wall -Wextra -Werror)
target_include_directories(ui-composition-test PRIVATE ..)
add_executable(settings-test EXCLUDE_FROM_ALL ../tests/test_settings.cpp shim/pes_settings.cpp)
target_compile_options(settings-test PRIVATE -UNDEBUG -Wall -Wextra -Werror)
target_include_directories(settings-test PRIVATE .. shim)

add_executable(scene-resolution-test EXCLUDE_FROM_ALL ../tests/test_scene_resolution.cpp ../tests/test_gpu_runtime_stubs.cpp)
target_include_directories(scene-resolution-test PRIVATE shim shadps4 third_party/gcn/include)
# Vulkan-Hpp's dispatch base changes layout with NDEBUG. Match the production ABI;
# these GPU tests keep their assertions separately through test_assert.h.
# The test's Vulkan-Hpp inline methods have a different set of platform extensions
# from vk_platform.cpp. Keep them local and use an explicit dispatcher in the test.
target_compile_options(scene-resolution-test PRIVATE -fPIC)
target_compile_definitions(scene-resolution-test PRIVATE NDEBUG)
set_source_files_properties(../tests/test_scene_resolution.cpp PROPERTIES
    COMPILE_OPTIONS "-fvisibility=hidden;-fvisibility-inlines-hidden")
target_link_libraries(scene-resolution-test PRIVATE pesgpu)
add_executable(taa-shader-test EXCLUDE_FROM_ALL ../tests/test_taa_shader.cpp ../tests/test_gpu_runtime_stubs.cpp)
target_include_directories(taa-shader-test PRIVATE shim shadps4 ${SHADER_INCLUDE} third_party/gcn/include)
target_compile_options(taa-shader-test PRIVATE -fPIC)
target_compile_definitions(taa-shader-test PRIVATE NDEBUG)
set_source_files_properties(../tests/test_taa_shader.cpp PROPERTIES
    COMPILE_OPTIONS "-fvisibility=hidden;-fvisibility-inlines-hidden")
target_link_libraries(taa-shader-test PRIVATE pesgpu)
add_dependencies(taa-shader-test host_shaders)

add_executable(camera-motion-test EXCLUDE_FROM_ALL ../tests/test_camera_motion.cpp ../tests/test_gpu_runtime_stubs.cpp)
target_include_directories(camera-motion-test PRIVATE shim shadps4 ${SHADER_INCLUDE} third_party/gcn/include)
target_compile_options(camera-motion-test PRIVATE -fPIC)
target_compile_definitions(camera-motion-test PRIVATE NDEBUG)
target_link_libraries(camera-motion-test PRIVATE pesgpu)
add_dependencies(camera-motion-test host_shaders)

# Large deferred payload ownership/regression, adapted from AwesomeObserver/windows.
add_executable(dlss-model-test EXCLUDE_FROM_ALL ../tests/test_dlss_model.cpp ../tests/test_gpu_runtime_stubs.cpp)
target_include_directories(dlss-model-test PRIVATE shim shadps4 third_party/gcn/include)
target_compile_definitions(dlss-model-test PRIVATE NDEBUG)
target_link_libraries(dlss-model-test PRIVATE pesgpu)
if(WIN32)
    target_sources(dlss-model-test PRIVATE ../src/win32_crash.c)
endif()

add_executable(record-data-test EXCLUDE_FROM_ALL ../tests/test_record_data.cpp ../tests/test_gpu_runtime_stubs.cpp)
target_include_directories(record-data-test PRIVATE shim shadps4 third_party/gcn/include)
target_compile_definitions(record-data-test PRIVATE NDEBUG)
target_link_libraries(record-data-test PRIVATE pesgpu)
enable_testing()
foreach(test IN ITEMS liverpool-tasks early-z)
    string(REPLACE "-" "_" test_source ${test})
    add_executable(${test}-test EXCLUDE_FROM_ALL ../tests/test_${test_source}.cpp ../tests/test_gpu_runtime_stubs.cpp)
    target_include_directories(${test}-test PRIVATE shim shadps4 third_party/gcn/include)
    target_compile_options(${test}-test PRIVATE -UNDEBUG)
    target_link_libraries(${test}-test PRIVATE pesgpu)
    if(WIN32)
        target_link_libraries(${test}-test PRIVATE winmm ws2_32 psapi onecore)
    endif()
    add_test(NAME ${test} COMMAND ${test}-test)
    set_tests_properties(${test} PROPERTIES TIMEOUT 120)
endforeach()
foreach(mode IN ITEMS async sync direct)
    add_test(NAME record-data-${mode} COMMAND record-data-test)
    if(mode STREQUAL "async")
        set(record_mode_env "PES_ASYNC_SUBMIT=1;PES_VK_RECORD_THREAD=1")
    elseif(mode STREQUAL "sync")
        set(record_mode_env "PES_ASYNC_SUBMIT=0;PES_VK_RECORD_THREAD=1")
    else()
        set(record_mode_env "PES_ASYNC_SUBMIT=0;PES_VK_RECORD_THREAD=0")
    endif()
    set_tests_properties(record-data-${mode} PROPERTIES TIMEOUT 120 ENVIRONMENT "${record_mode_env};PES_VK_RECORD_THREADS=3;PES_VK_SEGMENT_KB=1;PES_PRODUCER_CHECK=1")
endforeach()

# pes: FSR 4 benchmark outside the game (tools/fsr4_bench.cpp): ninja -C out/gpu fsr4-bench.
add_executable(upscaler-support-test EXCLUDE_FROM_ALL ../tests/test_upscaler_support.cpp
    shim/pes_settings.cpp)
target_include_directories(upscaler-support-test PRIVATE shim)
target_compile_options(upscaler-support-test PRIVATE -UNDEBUG)

add_executable(fsr4-bench EXCLUDE_FROM_ALL tools/fsr4_bench.cpp
    shadps4/video_core/renderer_vulkan/fsr411/fsr411.cpp)
target_include_directories(fsr4-bench PRIVATE shadps4/video_core/renderer_vulkan/fsr411)
target_link_libraries(fsr4-bench PRIVATE ffx-vulkan::fsr4-v07-vulkan ffx-vulkan::fsr4-v07-assets Vulkan::Vulkan)



if(WIN32)
    add_executable(trophies-test EXCLUDE_FROM_ALL ../tests/test_trophies.c)
    target_include_directories(trophies-test PRIVATE .. ../src)
    target_compile_options(trophies-test PRIVATE -UNDEBUG -O2 -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
    target_link_options(trophies-test PRIVATE -Wl,--gc-sections)
    add_test(NAME trophies COMMAND trophies-test)
endif()


# Frame pacing is tested without launching the game or initializing a GPU.
add_executable(frame-pacing-test EXCLUDE_FROM_ALL ../tests/test_frame_pacing.cpp shim/frame_pacing.cpp)
target_include_directories(frame-pacing-test PRIVATE shim shadps4 ../tests)
target_compile_options(frame-pacing-test PRIVATE -UNDEBUG -Wall -Wextra -Werror)
add_test(NAME frame-pacing COMMAND frame-pacing-test)

