// SPDX-License-Identifier: GPL-2.0-or-later
// pes_settings.cpp: the menu saves its keys into pes17recomp.ini and keeps the launcher's (controls).
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <sstream>
#include <string>
#include "gpu/shim/pes_settings.h"

static std::string Read(const char* path) {
    std::ifstream file(path);
    std::stringstream text;
    text << file.rdbuf();
    return text.str();
}

int main() {
    const auto temporary = std::filesystem::temp_directory_path() /
        ("pes-settings-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const std::string filename = temporary.string();
    const char* path = filename.c_str();
    const char ini[] = "# launcher\nupscaler=fsr3\nkey.cross=X, Space\npad.circle=a\nshow_fps=0\n"
                       "fullscreen_hint=1\n";
    { std::ofstream file(path); file << ini; assert(file.good()); }
#ifdef _WIN32
    _putenv_s("PES_CONFIG", path);
#else
    setenv("PES_CONFIG", path, 1);
#endif

    PesSettings::Load();
    auto& s = PesSettings::Get();
    for(int model:PesSettings::DlssPresets) {
        assert(PesSettings::ParseDlssPreset(PesSettings::DlssPresetName(model))==model);
        assert(PesSettings::ParseDlssPreset(std::to_string(model))==model);
        s.dlss_preset=model;
        s.preset=PesSettings::Balanced;
        PesSettings::Save(); s.dlss_preset=0; PesSettings::Load();
        assert(s.dlss_preset==model && s.preset==PesSettings::Balanced);
    }
    assert(PesSettings::ParseDlssPreset("  m ")==13);
    for(const char* bad:{"7","8","9","14","15","-1","11x","unknown"})
        assert(PesSettings::ParseDlssPreset(bad)==0);
    assert(s.upscaler == PesSettings::UpscalerFsr3 && !s.show_fps && s.menu_x < 0.0f);
    s.show_fps = true;
    s.upscaler = PesSettings::UpscalerFsr411;
    s.menu_x = 0.625f;
    s.menu_y = 0.125f;
    s.fullscreen = true;
    PesSettings::Save();

    const std::string saved = Read(path);
    // The launcher's controls, its other keys and comments stay; the menu's keys are replaced
    // in place, new ones appended.
    assert(saved.find("# launcher\nupscaler=fsr411\nkey.cross=X, Space\npad.circle=a\nshow_fps=1\n"
                      "fullscreen_hint=1\n") == 0);
    assert(saved.find("menu_pos=0.6250,0.1250\n") != std::string::npos);
    assert(saved.find("upscaler=fsr3") == std::string::npos);
    assert(saved.find("fullscreen=1\n") != std::string::npos);

    s.menu_x = -1.0f;
    s.menu_y = -1.0f;
    PesSettings::Load();
    assert(s.menu_x == 0.625f && s.menu_y == 0.125f && s.upscaler == PesSettings::UpscalerFsr411);
    // Replacing an existing file works on Windows too, and must not erase key bindings.
    s.upscaler = PesSettings::UpscalerDlss;
    PesSettings::Save();
    assert(Read(path).find("upscaler=dlss\n") != std::string::npos);
    assert(Read(path).find("key.cross=X, Space\n") != std::string::npos);
    PesSettings::ConfigureUpscalerSupport(false, false, false);
    assert(s.upscaler == PesSettings::UpscalerFsr3 && !s.dlss_supported);
    s.upscaler = PesSettings::UpscalerDlss;
    PesSettings::ConfigureUpscalerSupport(false, false, true);
    assert(s.upscaler == PesSettings::UpscalerDlss && s.dlss_supported);
    s.camera_controls = true;
    s.camera_fov_scale = 1.25f;
    s.camera_distance_scale = 0.80f;
    s.camera_height_scale = 1.10f;
    PesSettings::Save();
    s.camera_controls = false;
    s.camera_fov_scale = s.camera_distance_scale = s.camera_height_scale = 1.0f;
    PesSettings::Load();
    assert(s.camera_controls && s.camera_fov_scale == 1.25f);
    assert(s.camera_distance_scale == 0.80f && s.camera_height_scale == 1.10f);
    s.change_appearance=true;s.rebirth=true;s.graphics_controls=true;
    s.graphics_bloom=0.7f;s.graphics_shadow_scale=1.4f;
    PesSettings::Save();
    s.rebirth=false;s.graphics_bloom=1.0f;
    PesSettings::Load();
    assert(s.change_appearance && s.rebirth && s.graphics_controls && s.graphics_bloom==0.7f && s.graphics_shadow_scale==1.4f);
    // Edited configs must not feed infinite/NaN camera transforms to the engine.
    { std::ofstream file(path); file << "camera_fov_scale=nan\ncamera_distance_scale=-10\ncamera_height_scale=inf\n"; }
    PesSettings::Load();
    assert(s.camera_fov_scale == 1.0f && s.camera_distance_scale == 0.5f && s.camera_height_scale == 1.0f);
    std::filesystem::remove(temporary);
    std::puts("PASS: settings save keeps other keys, menu position");
}
