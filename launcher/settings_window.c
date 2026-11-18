/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Settings window shell (M6.5a step A4): a fixed-size window with the five tabs (Display, Keyboard,
 * Controller, Audio, Online; Specifications and Support are skipped by decision), OK and Cancel.
 * The tab contents are added in the next steps. Size and placement follow the reference screenshots
 * in the reference screenshots (800 x 747 including the title bar); the window never resizes. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>
#include "settings_display.h"

#define CLIENT_WIDTH 800
#define CLIENT_HEIGHT 716
#define ID_TABS 100
#define ID_OK 101
#define ID_CANCEL 102

static const wchar_t *kClassName = L"Pes17SettingsWindow";
static const wchar_t *kTabNames[] = {L"Display", L"Keyboard", L"Controller", L"Audio", L"Online"};
static const int kTabCount = (int)(sizeof(kTabNames) / sizeof(kTabNames[0]));
static HWND g_tabs, g_ok, g_cancel;

/* Returns the rectangle inside the tab control where the tab pages go (client coordinates). */
static RECT PageRect(void) {
    RECT r = {24, 150, CLIENT_WIDTH - 24, CLIENT_HEIGHT - 60};
    return r;
}

static void LayoutControls(HWND hwnd) {
    (void)hwnd;
    RECT page = PageRect();
    SetWindowPos(g_tabs, NULL, page.left, page.top - 26, page.right - page.left, page.bottom - page.top + 26,
                 SWP_NOZORDER);
    SetWindowPos(g_ok, NULL, 510, CLIENT_HEIGHT - 44, 126, 30, SWP_NOZORDER);
    SetWindowPos(g_cancel, NULL, 648, CLIENT_HEIGHT - 44, 126, 30, SWP_NOZORDER);
}

static void CreateControls(HWND hwnd) {
    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_TAB_CLASSES | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);
    g_tabs = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 0, 0, hwnd,
                             (HMENU)(INT_PTR)ID_TABS, GetModuleHandleW(NULL), NULL);
    for (int i = 0; i < kTabCount; ++i) {
        TCITEMW item = {0};
        item.mask = TCIF_TEXT;
        item.pszText = (LPWSTR)kTabNames[i];
        TabCtrl_InsertItem(g_tabs, i, &item);
    }
    g_ok = CreateWindowExW(0, L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 0, 0, 0, 0,
                           hwnd, (HMENU)(INT_PTR)ID_OK, GetModuleHandleW(NULL), NULL);
    g_cancel = CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 0, 0, hwnd,
                               (HMENU)(INT_PTR)ID_CANCEL, GetModuleHandleW(NULL), NULL);
    /* Default system font, as the reference uses the Windows UI font. */
    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    SendMessageW(g_tabs, WM_SETFONT, (WPARAM)font, TRUE);
    SendMessageW(g_ok, WM_SETFONT, (WPARAM)font, TRUE);
    SendMessageW(g_cancel, WM_SETFONT, (WPARAM)font, TRUE);
    SettingsDisplayCreate(hwnd, font);
    SettingsDisplayShow(TabCtrl_GetCurSel(g_tabs) == 0);
    LayoutControls(hwnd);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        CreateControls(hwnd);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wp) == ID_CANCEL) {
            DestroyWindow(hwnd);
        } else if (LOWORD(wp) == ID_OK) {
            /* Saving is added in step A9 (launcher hook-up); for now OK closes the window. */
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_NOTIFY: {
        NMHDR *nm = (NMHDR *)lp;
        if (nm->idFrom == ID_TABS && nm->code == TCN_SELCHANGE) {
            SettingsDisplayShow(TabCtrl_GetCurSel(g_tabs) == 0);
        }
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, LPWSTR cmdline, int show) {
    (void)prev;
    (void)cmdline;
    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);

    /* Fixed size: no thick frame and no maximise box, so the window cannot be resized or dragged
     * to another size. The window is sized from the client area, so the controls keep their places. */
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT frame = {0, 0, CLIENT_WIDTH, CLIENT_HEIGHT};
    AdjustWindowRect(&frame, style, FALSE);
    HWND hwnd = CreateWindowExW(0, kClassName, L"Settings", style, CW_USEDEFAULT, CW_USEDEFAULT,
                                frame.right - frame.left, frame.bottom - frame.top, NULL, NULL, inst, NULL);
    if (!hwnd) return 1;
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    MSG m;
    while (GetMessageW(&m, NULL, 0, 0) > 0) {
        if (!IsDialogMessageW(hwnd, &m)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
    }
    return (int)m.wParam;
}
