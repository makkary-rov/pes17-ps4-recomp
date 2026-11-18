/* SPDX-License-Identifier: GPL-2.0-or-later */
/* The Display tab of the settings window (M6.5a step A5): screen mode, resolution, quality, and the
 * vsync and frame-rate settings. The values live in settings_model (tested on every platform); this
 * file only creates the controls and moves values between them and the model. Positions are client
 * coordinates of the settings window, estimated from the reference screenshots (see settings_window.c). */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include "settings_model.h"
#include "settings_display.h"

enum {
    ID_MODE_WINDOWED = 300, ID_MODE_BORDERLESS, ID_MODE_FULLSCREEN,
    ID_RESOLUTION,
    ID_QUALITY_LOW, ID_QUALITY_MEDIUM, ID_QUALITY_HIGH,
    ID_VSYNC, ID_FPS_INGAME, ID_FPS_REPLAY,
};

static HWND g_page_controls[32];
static int g_page_count;
static HWND g_mode[3], g_resolution, g_quality[3], g_vsync, g_fps_ingame, g_fps_replay;

static HWND Make(HWND parent, const wchar_t *cls, const wchar_t *text, DWORD style, int x, int y, int w, int h, int id) {
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, parent,
                             (HMENU)(INT_PTR)id, GetModuleHandleW(NULL), NULL);
    if (g_page_count < (int)(sizeof(g_page_controls) / sizeof(g_page_controls[0]))) {
        g_page_controls[g_page_count++] = c;
    }
    return c;
}

static void GroupBox(HWND parent, const wchar_t *title, int x, int y, int w, int h) {
    Make(parent, L"BUTTON", title, BS_GROUPBOX, x, y, w, h, 0);
}

void SettingsDisplayCreate(HWND parent, HFONT font) {
    g_page_count = 0;
    /* Screen mode: three choices (the original has two; borderless is the port's current fullscreen). */
    GroupBox(parent, L"Screen Mode", 40, 160, 718, 84);
    g_mode[0] = Make(parent, L"BUTTON", L"Windowed", BS_AUTORADIOBUTTON | WS_GROUP | WS_TABSTOP, 64, 190, 180, 24, ID_MODE_WINDOWED);
    g_mode[1] = Make(parent, L"BUTTON", L"Borderless", BS_AUTORADIOBUTTON | WS_TABSTOP, 300, 190, 180, 24, ID_MODE_BORDERLESS);
    g_mode[2] = Make(parent, L"BUTTON", L"Fullscreen", BS_AUTORADIOBUTTON | WS_TABSTOP, 540, 190, 180, 24, ID_MODE_FULLSCREEN);

    /* Resolution: the fixed list from 640x480 to 1920x1080. */
    GroupBox(parent, L"Resolution", 40, 262, 718, 84);
    g_resolution = Make(parent, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 64, 292, 670, 240, ID_RESOLUTION);
    for (int i = 0; i < PES_RESOLUTION_COUNT; ++i) {
        wchar_t label[64];
        swprintf(label, 64, L"%S", PesResolutions[i].label);
        SendMessageW(g_resolution, CB_ADDSTRING, 0, (LPARAM)label);
    }

    /* Quality: saved only, no effect yet (user decision). */
    GroupBox(parent, L"Quality", 40, 364, 718, 84);
    g_quality[0] = Make(parent, L"BUTTON", L"Low", BS_AUTORADIOBUTTON | WS_GROUP | WS_TABSTOP, 64, 394, 200, 24, ID_QUALITY_LOW);
    g_quality[1] = Make(parent, L"BUTTON", L"Medium", BS_AUTORADIOBUTTON | WS_TABSTOP, 300, 394, 200, 24, ID_QUALITY_MEDIUM);
    g_quality[2] = Make(parent, L"BUTTON", L"High", BS_AUTORADIOBUTTON | WS_TABSTOP, 540, 394, 200, 24, ID_QUALITY_HIGH);

    /* Vertical sync and frame rates (PES 2018 standard). */
    GroupBox(parent, L"Vertical Sync and Frame Rate", 40, 466, 718, 118);
    g_vsync = Make(parent, L"BUTTON", L"Vertical sync", BS_AUTOCHECKBOX | WS_TABSTOP, 64, 496, 300, 24, ID_VSYNC);
    Make(parent, L"STATIC", L"Frame rate in game", SS_LEFT, 64, 530, 200, 20, 0);
    g_fps_ingame = Make(parent, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 300, 528, 120, 120, ID_FPS_INGAME);
    Make(parent, L"STATIC", L"Frame rate in replays", SS_LEFT, 64, 556, 200, 20, 0);
    g_fps_replay = Make(parent, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 300, 554, 120, 120, ID_FPS_REPLAY);
    SendMessageW(g_fps_ingame, CB_ADDSTRING, 0, (LPARAM)L"60");
    SendMessageW(g_fps_ingame, CB_ADDSTRING, 0, (LPARAM)L"30");
    SendMessageW(g_fps_replay, CB_ADDSTRING, 0, (LPARAM)L"30");
    SendMessageW(g_fps_replay, CB_ADDSTRING, 0, (LPARAM)L"60");

    for (int i = 0; i < g_page_count; ++i) SendMessageW(g_page_controls[i], WM_SETFONT, (WPARAM)font, TRUE);
    SettingsDisplaySetValues(PesDisplayDefaults());
}

void SettingsDisplayShow(int visible) {
    for (int i = 0; i < g_page_count; ++i) ShowWindow(g_page_controls[i], visible ? SW_SHOW : SW_HIDE);
}

/* Moves the model's values onto the controls. */
void SettingsDisplaySetValues(PesDisplaySettings s) {
    s = PesDisplayClamp(s);
    for (int i = 0; i < 3; ++i) SendMessageW(g_mode[i], BM_SETCHECK, i == s.display_mode ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(g_resolution, CB_SETCURSEL, s.resolution, 0);
    for (int i = 0; i < 3; ++i) SendMessageW(g_quality[i], BM_SETCHECK, i == s.quality ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(g_vsync, BM_SETCHECK, s.vsync ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(g_fps_ingame, CB_SETCURSEL, s.fps_ingame == 60 ? 0 : 1, 0);
    SendMessageW(g_fps_replay, CB_SETCURSEL, s.fps_replay == 30 ? 0 : 1, 0);
}

/* Reads the controls into a model (the caller clamps and saves it). */
PesDisplaySettings SettingsDisplayGetValues(void) {
    PesDisplaySettings s = PesDisplayDefaults();
    for (int i = 0; i < 3; ++i) if (SendMessageW(g_mode[i], BM_GETCHECK, 0, 0) == BST_CHECKED) s.display_mode = i;
    s.resolution = (int)SendMessageW(g_resolution, CB_GETCURSEL, 0, 0);
    for (int i = 0; i < 3; ++i) if (SendMessageW(g_quality[i], BM_GETCHECK, 0, 0) == BST_CHECKED) s.quality = i;
    s.vsync = SendMessageW(g_vsync, BM_GETCHECK, 0, 0) == BST_CHECKED;
    s.fps_ingame = SendMessageW(g_fps_ingame, CB_GETCURSEL, 0, 0) == 0 ? 60 : 30;
    s.fps_replay = SendMessageW(g_fps_replay, CB_GETCURSEL, 0, 0) == 0 ? 30 : 60;
    return PesDisplayClamp(s);
}
