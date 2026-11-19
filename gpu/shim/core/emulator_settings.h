// SPDX-License-Identifier: GPL-2.0-or-later
// pes: shadPS4 settings used by the video core, read once from PES_* environment
// variables (defaults match shadPS4 except the pipeline cache, which is on).
#pragma once
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include "common/types.h"

enum GpuReadbacksMode : int { Disabled, Relaxed, Precise };

// pes: the settings file's Display values (pes_settings.cpp): the in-game frame rate (60 or 30) and
// vertical sync. The PES_* environment variables still override them.
long PesFpsInGame();   // gameplay and menus (the game asks for 60): 30, 60 or 120
long PesFpsCutscene(); // cutscenes (the game asks for 30): 30 or 60
bool PesVsync();

class EmulatorSettingsImpl {
public:
    static EmulatorSettingsImpl* GetInstance() { static EmulatorSettingsImpl s; return &s; }
    static bool Flag(const char* name, bool fallback) {
        const char* v = std::getenv(name);
        return v ? (v[0] == '1' || v[0] == 'y' || v[0] == 't') : fallback;
    }
    static long Number(const char* name, long fallback) {
        const char* v = std::getenv(name);
        return v ? std::strtol(v, nullptr, 10) : fallback;
    }
    s32 GetGpuId() { static const auto value = s32(Number("PES_GPU_ID", -1)); return value; }
    u32 GetInternalScreenWidth() { static const auto value = u32(Number("PES_INTERNAL_WIDTH", 1920)); return value; }
    u32 GetInternalScreenHeight() { static const auto value = u32(Number("PES_INTERNAL_HEIGHT", 1080)); return value; }
    bool RequiresFifoForFrameRate() {
        const char* fps = std::getenv("PES_FPS");
        return (fps && (!std::strcmp(fps, "120") || !std::strcmp(fps, "uncap"))) ||
               GetFrameLimit() == 0 || GetFrameLimit() == 120 || GetVblankFrequency() >= 120;
    }
    std::string GetPresentMode() {
#ifdef _WIN32
        // Keep high-FPS presentation synchronized even for direct runtime launches
        // and older launchers that still request Mailbox/Immediate/Relaxed.
        if (RequiresFifoForFrameRate()) return "Fifo";
#endif
        // pes: "Wait for vertical sync" in the settings file: Mailbox (synchronized) when on, Immediate
        // (no sync, may tear) when off.
        const char* v = std::getenv("PES_PRESENT_MODE");
        return v ? v : (PesVsync() ? "Mailbox" : "Immediate");
    }
    int GetRcasAttenuation() { static const auto value = int(Number("PES_RCAS_ATTENUATION", 250)); return value; }
    // pes: Relaxed by default: without readbacks FaceGen reads stale GPU-written vertices
    // (vertex explosions); in a heavy upstream-title scene it costs no measurable frame rate.
    u32 GetReadbacksMode() { static const auto value = u32(Number("PES_READBACKS", GpuReadbacksMode::Relaxed)); return value; }
    // pes: uncapped presets (run.sh: PES_VBLANK_HZ=480; 0 means the same): vblank runs at
    // 480 Hz, and a finished frame is presented as soon as it arrives (IsUncappedVblank), not on
    // the next vblank tick (at the display rate frames alternated 10/20 ms at 100 Hz: judder; at
    // 480 Hz they snap to ~2.1 ms steps). The frame rate is not limited unless PES_FPS_LIMIT
    // (launcher: "FPS limit") asks for it. Other values are used as given (60/90: the
    // fixed-timestep presets, flips on vblank ticks).
    u32 GetVblankFrequency() {
        static const u32 value = [] {
            // pes: 120 Hz when the settings ask for 120 FPS gameplay, else the PS4's 60 Hz.
            const long hz = Number("PES_VBLANK_HZ", PesFpsInGame() == 120 ? 120 : 60);
#ifdef _WIN32
            // Uncapped flips use the fine vblank clock; the independent title guard stays active.
            return hz == 0 || hz == 480 ? 480u :
                   hz == 30 || hz == 60 || hz == 90 || hz == 120 ? u32(hz) : 60u;
#else
            return hz > 0 ? u32(hz) : 480u;
#endif
        }();
        return value;
    }
    /// Vblank faster than any fixed-timestep preset (above 120 Hz, or 0): flips at once.
    bool IsUncappedVblank() {
        static const bool value = [] {
            const long hz = Number("PES_VBLANK_HZ", 60);
#ifdef _WIN32
            return hz == 0 || hz == 480;
#else
            return hz <= 0 || hz > 120;
#endif
        }();
        return value;
    }
    /// Frames per second the present thread lets through; 0 = no limit (PES_FPS_LIMIT).
    u32 GetFrameLimit() {
        static const u32 value = [] {
#ifdef _WIN32
            // pes: the highest frame rate the settings ask for (gameplay or cutscenes) unless
            // PES_FPS_LIMIT is set; the present thread picks the rate of each scene (driver.cpp).
            const long fps = Number("PES_FPS_LIMIT", std::max(PesFpsInGame(), PesFpsCutscene()));
            return fps == 0 || fps == 30 || fps == 60 || fps == 90 || fps == 120 ? u32(fps) : 60u;
#else
            return u32(std::max(0L, Number("PES_FPS_LIMIT", 0)));
#endif
        }();
        return value;
    }
    bool IsCopyGpuBuffers() { static const auto value = Flag("PES_COPY_GPU_BUFFERS", false); return value; }
    bool IsDirectMemoryAccessEnabled() { static const auto value = Flag("PES_DIRECT_MEMORY_ACCESS", false); return value; }
    bool IsDumpShaders() { static const auto value = Flag("PES_DUMP_SHADERS", false); return value; }
    bool IsFsrEnabled() { static const auto value = Flag("PES_FSR1", false); return value; }
    bool IsHdrAllowed() { static const auto value = Flag("PES_HDR", false); return value; }
    bool IsNullGPU() { static const auto value = Flag("PES_NULL_GPU", false); return value; }
    bool IsPatchShaders() { return false; }
    bool IsPipelineCacheArchived() { return false; }
    bool IsPipelineCacheEnabled() { static const auto value = Flag("PES_PIPELINE_CACHE", true); return value; }
    bool IsRcasEnabled() { static const auto value = Flag("PES_RCAS", true); return value; }
    bool IsReadbackLinearImagesEnabled() { static const auto value = Flag("PES_READBACK_LINEAR", true); return value; }
    bool IsRenderdocEnabled() { return false; }
    bool IsShaderCollect() { return false; }
    bool IsUserfaultfdTracking() { return false; }
    bool IsVkCrashDiagnosticEnabled() { return false; }
    bool IsVkGuestMarkersEnabled() { static const auto value = Flag("PES_VK_MARKERS", false); return value; }
    bool IsVkHostMarkersEnabled() { static const auto value = Flag("PES_VK_MARKERS", false); return value; }
    bool IsVkValidationCoreEnabled() { return true; }
    bool IsVkValidationEnabled() { static const auto value = Flag("PES_VK_VALIDATION", false); return value; }
    bool IsVkValidationGpuEnabled() { return false; }
    bool IsVkValidationSyncEnabled() { static const auto value = Flag("PES_VK_VALIDATION_SYNC", false); return value; }
};
#define EmulatorSettings (*EmulatorSettingsImpl::GetInstance())
