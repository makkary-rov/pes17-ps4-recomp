/* SPDX-License-Identifier: GPL-2.0-or-later */
/* The Display tab of the settings window (M6.5a step A5): screen mode, resolution, quality, the
 * "Other settings" group (vertical sync, frame rates, motion blur) and the FSR 3.1 group. The values
 * live in settings_model (tested on every platform); this file creates the controls and moves values
 * between them and the model.
 *
 * The controls sit in a page panel that scrolls vertically when its content is taller than the page.
 * Scrolling moves every control to its own position (no pixel shift of the page), then repaints the
 * whole page, so no stale pixels are left behind. Positions are estimated from the reference
 * screenshots and need a visual check. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "settings_model.h"
#include "settings_display.h"

#define PAGE_X 24
#define PAGE_Y 150
#define PAGE_W 752
#define PAGE_H 506
#define MAX_CHILDREN 64

enum {
    ID_MODE_WINDOWED = 300, ID_MODE_BORDERLESS, ID_MODE_FULLSCREEN,
    ID_RESOLUTION,
    ID_QUALITY_LOW, ID_QUALITY_MEDIUM, ID_QUALITY_HIGH,
    ID_VSYNC, ID_FPS_INGAME, ID_FPS_REPLAY, ID_MOTION_BLUR,
    ID_FSR_ON, ID_FSR_PRESET,
};

/* The page (viewport) shows a tall content panel that holds every control. Scrolling moves only the
 * content panel; the viewport clears and repaints the area it uncovers. */
static const wchar_t *kPageClass = L"PesSettingsPage";
static const wchar_t *kContentClass = L"PesSettingsContent";
static HWND g_page, g_content;
static int g_scroll_pos;
static int g_content_height;
static HWND g_mode[3], g_resolution, g_quality[3], g_vsync, g_fps_ingame, g_fps_replay, g_motion_blur;
static HWND g_fsr_on, g_fsr_preset;

static HWND Make(HWND parent, const wchar_t *cls, const wchar_t *text, DWORD style, int x, int y, int w, int h, int id) {
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, parent,
                             (HMENU)(INT_PTR)id, GetModuleHandleW(NULL), NULL);
    if (y + h > g_content_height) g_content_height = y + h;
    return c;
}

static void GroupBox(HWND parent, const wchar_t *title, int x, int y, int w, int h) {
    Make(parent, L"BUTTON", title, BS_GROUPBOX, x, y, w, h, 0);
}

/* Scroll range follows the content; the bar only shows when the content is taller than the page. */
static void UpdateScroll(HWND page) {
    RECT rc;
    GetClientRect(page, &rc);
    const int page_h = rc.bottom - rc.top;
    if (g_content_height <= page_h) {
        g_scroll_pos = 0;
        ShowScrollBar(page, SB_VERT, FALSE);
        return;
    }
    SCROLLINFO si;
    memset(&si, 0, sizeof(si));
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = g_content_height - 1;
    si.nPage = (UINT)page_h;
    si.nPos = g_scroll_pos;
    SetScrollInfo(page, SB_VERT, &si, TRUE);
    ShowScrollBar(page, SB_VERT, TRUE);
}

/* Moves the content panel up by pos pixels and repaints the viewport (clears the uncovered strip). */
static void ApplyScroll(HWND page, int pos) {
    RECT rc;
    GetClientRect(page, &rc);
    const int max_pos = g_content_height - (rc.bottom - rc.top);
    if (max_pos <= 0) pos = 0;
    if (pos < 0) pos = 0;
    if (max_pos > 0 && pos > max_pos) pos = max_pos;
    g_scroll_pos = pos;
    SCROLLINFO si;
    memset(&si, 0, sizeof(si));
    si.cbSize = sizeof(si);
    si.fMask = SIF_POS;
    si.nPos = pos;
    SetScrollInfo(page, SB_VERT, &si, TRUE);
    SetWindowPos(g_content, NULL, 0, -pos, PAGE_W, g_content_height, SWP_NOZORDER | SWP_NOACTIVATE);
    InvalidateRect(page, NULL, TRUE);
    UpdateWindow(page);
}

static LRESULT CALLBACK PageProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_SIZE:
        UpdateScroll(hwnd);
        ApplyScroll(hwnd, g_scroll_pos);
        return 0;
    case WM_VSCROLL: {
        SCROLLINFO si;
        memset(&si, 0, sizeof(si));
        si.cbSize = sizeof(si);
        si.fMask = SIF_ALL;
        GetScrollInfo(hwnd, SB_VERT, &si);
        int pos = si.nPos;
        const int step = 20;
        switch (LOWORD(wp)) {
        case SB_LINEUP: pos -= step; break;
        case SB_LINEDOWN: pos += step; break;
        case SB_PAGEUP: pos -= (int)si.nPage; break;
        case SB_PAGEDOWN: pos += (int)si.nPage; break;
        case SB_THUMBTRACK: case SB_THUMBPOSITION: pos = si.nTrackPos; break;
        default: return 0;
        }
        ApplyScroll(hwnd, pos);
        return 0;
    }
    case WM_MOUSEWHEEL: {
        const int notches = GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA;
        ApplyScroll(hwnd, g_scroll_pos - notches * 40);
        return 0;
    }
    case WM_COMMAND:
        /* The FSR mode is only usable while FSR 3.1 is on. */
        if (LOWORD(wp) == ID_FSR_ON && HIWORD(wp) == BN_CLICKED) {
            EnableWindow(g_fsr_preset, SendMessageW(g_fsr_on, BM_GETCHECK, 0, 0) == BST_CHECKED);
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void RegisterPesClass(const wchar_t *name, WNDPROC proc) {
    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    if (GetClassInfoExW(GetModuleHandleW(NULL), name, &wc)) return;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = proc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = name;
    RegisterClassExW(&wc);
}

void SettingsDisplayCreate(HWND parent, HFONT font) {
    RegisterPesClass(kPageClass, PageProc);
    RegisterPesClass(kContentClass, DefWindowProcW);
    g_content_height = 0;
    g_scroll_pos = 0;
    g_page = CreateWindowExW(0, kPageClass, L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_VSCROLL,
                             PAGE_X, PAGE_Y, PAGE_W, PAGE_H, parent, NULL, GetModuleHandleW(NULL), NULL);
    /* Every control lives in the content panel; the content height is set once they are all made. */
    g_content = CreateWindowExW(0, kContentClass, L"", WS_CHILD | WS_VISIBLE, 0, 0, PAGE_W, PAGE_H,
                                g_page, NULL, GetModuleHandleW(NULL), NULL);
    HWND p = g_content;
    /* Screen mode: three choices (the original has two; borderless is the port's current fullscreen). */
    GroupBox(p, L"Screen Mode", 16, 10, 718, 84);
    g_mode[0] = Make(p, L"BUTTON", L"Windowed", BS_AUTORADIOBUTTON | WS_GROUP | WS_TABSTOP, 40, 40, 180, 24, ID_MODE_WINDOWED);
    g_mode[1] = Make(p, L"BUTTON", L"Borderless", BS_AUTORADIOBUTTON | WS_TABSTOP, 276, 40, 180, 24, ID_MODE_BORDERLESS);
    g_mode[2] = Make(p, L"BUTTON", L"Fullscreen", BS_AUTORADIOBUTTON | WS_TABSTOP, 516, 40, 180, 24, ID_MODE_FULLSCREEN);

    /* Resolution: the fixed list from 640x480 to 1920x1080. */
    GroupBox(p, L"Resolution", 16, 112, 718, 84);
    g_resolution = Make(p, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 40, 142, 670, 240, ID_RESOLUTION);
    for (int i = 0; i < PES_RESOLUTION_COUNT; ++i) {
        wchar_t label[64];
        swprintf(label, 64, L"%S", PesResolutions[i].label);
        SendMessageW(g_resolution, CB_ADDSTRING, 0, (LPARAM)label);
    }

    /* Quality: saved only, no effect yet (user decision). */
    GroupBox(p, L"Quality", 16, 214, 718, 84);
    g_quality[0] = Make(p, L"BUTTON", L"Low", BS_AUTORADIOBUTTON | WS_GROUP | WS_TABSTOP, 40, 244, 200, 24, ID_QUALITY_LOW);
    g_quality[1] = Make(p, L"BUTTON", L"Medium", BS_AUTORADIOBUTTON | WS_TABSTOP, 276, 244, 200, 24, ID_QUALITY_MEDIUM);
    g_quality[2] = Make(p, L"BUTTON", L"High", BS_AUTORADIOBUTTON | WS_TABSTOP, 516, 244, 200, 24, ID_QUALITY_HIGH);

    /* Other settings: vertical sync, frame rates and motion blur (the PS4 native values). */
    GroupBox(p, L"Other settings", 16, 316, 718, 196);
    g_vsync = Make(p, L"BUTTON", L"Vertical sync", BS_AUTOCHECKBOX | WS_TABSTOP, 40, 346, 300, 24, ID_VSYNC);
    Make(p, L"STATIC", L"Frame rate in game", SS_LEFT, 40, 380, 200, 20, 0);
    g_fps_ingame = Make(p, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 276, 378, 120, 120, ID_FPS_INGAME);
    Make(p, L"STATIC", L"Frame rate in replays", SS_LEFT, 40, 406, 200, 20, 0);
    g_fps_replay = Make(p, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 276, 404, 120, 120, ID_FPS_REPLAY);
    SendMessageW(g_fps_ingame, CB_ADDSTRING, 0, (LPARAM)L"60");
    SendMessageW(g_fps_ingame, CB_ADDSTRING, 0, (LPARAM)L"30");
    SendMessageW(g_fps_replay, CB_ADDSTRING, 0, (LPARAM)L"30");
    SendMessageW(g_fps_replay, CB_ADDSTRING, 0, (LPARAM)L"60");
    Make(p, L"STATIC", L"Motion blur", SS_LEFT, 40, 448, 200, 20, 0);
    g_motion_blur = Make(p, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 276, 446, 120, 160, ID_MOTION_BLUR);
    SendMessageW(g_motion_blur, CB_ADDSTRING, 0, (LPARAM)L"OFF");
    SendMessageW(g_motion_blur, CB_ADDSTRING, 0, (LPARAM)L"25%");
    SendMessageW(g_motion_blur, CB_ADDSTRING, 0, (LPARAM)L"50%");
    SendMessageW(g_motion_blur, CB_ADDSTRING, 0, (LPARAM)L"75%");
    SendMessageW(g_motion_blur, CB_ADDSTRING, 0, (LPARAM)L"FULL");

    /* FSR 3.1: an on/off toggle and the quality mode (Ultra Performance to Quality). */
    GroupBox(p, L"FSR 3.1", 16, 528, 718, 110);
    g_fsr_on = Make(p, L"BUTTON", L"Enable FSR 3.1", BS_AUTOCHECKBOX | WS_TABSTOP, 40, 558, 300, 24, ID_FSR_ON);
    Make(p, L"STATIC", L"Mode", SS_LEFT, 40, 592, 200, 20, 0);
    g_fsr_preset = Make(p, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 276, 590, 200, 160, ID_FSR_PRESET);
    SendMessageW(g_fsr_preset, CB_ADDSTRING, 0, (LPARAM)L"Ultra Performance");
    SendMessageW(g_fsr_preset, CB_ADDSTRING, 0, (LPARAM)L"Performance");
    SendMessageW(g_fsr_preset, CB_ADDSTRING, 0, (LPARAM)L"Balanced");
    SendMessageW(g_fsr_preset, CB_ADDSTRING, 0, (LPARAM)L"Quality");

    /* Every control on the page takes the window font. */
    for (HWND c = GetWindow(p, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) SendMessageW(c, WM_SETFONT, (WPARAM)font, TRUE);
    SettingsDisplaySetValues(PesDisplayDefaults());
    UpdateScroll(g_page);
    ApplyScroll(g_page, 0);
}

void SettingsDisplayShow(int visible) {
    if (g_page) ShowWindow(g_page, visible ? SW_SHOW : SW_HIDE);
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
    int blur_index = 4;
    for (int i = 0; i < PES_MOTION_BLUR_COUNT; ++i) if (PesMotionBlurSteps[i] == s.motion_blur) blur_index = i;
    SendMessageW(g_motion_blur, CB_SETCURSEL, blur_index, 0);
    SendMessageW(g_fsr_on, BM_SETCHECK, s.fsr_enabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(g_fsr_preset, CB_SETCURSEL, s.fsr_preset, 0);
    EnableWindow(g_fsr_preset, s.fsr_enabled ? TRUE : FALSE);
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
    const int blur = (int)SendMessageW(g_motion_blur, CB_GETCURSEL, 0, 0);
    s.motion_blur = blur >= 0 && blur < PES_MOTION_BLUR_COUNT ? PesMotionBlurSteps[blur] : 0;
    s.fsr_enabled = SendMessageW(g_fsr_on, BM_GETCHECK, 0, 0) == BST_CHECKED;
    s.fsr_preset = (int)SendMessageW(g_fsr_preset, CB_GETCURSEL, 0, 0);
    return PesDisplayClamp(s);
}
