// SPDX-License-Identifier: GPL-2.0-or-later
// M6.5a settings keys: display mode, vsync, frame rates, audio buffer and quality. Defaults, round
// trip through pes17recomp.ini, an old fullscreen-only file, and rejection of bad values.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include "gpu/shim/pes_settings.h"

static std::string Read(const char* path) {
    std::ifstream file(path);
    std::stringstream text;
    text << file.rdbuf();
    return text.str();
}

static void UseConfig(const char* path) {
#ifdef _WIN32
    _putenv_s("PES_CONFIG", path);
#else
    setenv("PES_CONFIG", path, 1);
#endif
}

int main() {
    using namespace PesSettings;
    const char* defaults = "pes_keys_defaults.ini";
    std::remove(defaults);
    UseConfig(defaults);
    Load();
    auto& s = Get();
    assert(s.display_mode == DisplayWindowed && !s.fullscreen);
    assert(s.vsync && s.fps_ingame == 60 && s.fps_replay == 30);
    assert(s.audio_buffer == 1 && s.quality == 2);

    const char* path = "pes_keys_test.ini";
    { std::ofstream file(path); file << "fullscreen=1\nfps_ingame=30\nquality=medium\naudio_buffer=99\n"; }
    UseConfig(path);
    Load();
    assert(s.display_mode == DisplayBorderless && s.fullscreen);   /* old fullscreen file */
    assert(s.fps_ingame == 30);
    assert(s.quality == 1);
    assert(s.audio_buffer == 8);                                   /* clamped to 1..8 */

    s.display_mode = DisplayFullscreen;
    s.vsync = false;
    s.fps_replay = 60;
    s.quality = 0;
    Save();
    const std::string saved = Read(path);
    assert(saved.find("display_mode=fullscreen\n") != std::string::npos);
    assert(saved.find("fullscreen=1\n") != std::string::npos);
    assert(saved.find("vsync=0\n") != std::string::npos);
    assert(saved.find("fps_replay=60\n") != std::string::npos);
    assert(saved.find("quality=low\n") != std::string::npos);

    /* A bad frame rate keeps the value it had before this load (30 here); the other keys still load. */
    { std::ofstream file(path); file << "display_mode=windowed\nfps_ingame=45\n"; }
    Load();
    assert(s.display_mode == DisplayWindowed && !s.fullscreen);
    assert(s.fps_ingame == 30);
    std::printf("PASS: display mode, vsync, frame rates, audio buffer and quality keys\n");
    std::remove(path);
    std::remove(defaults);
    return 0;
}
