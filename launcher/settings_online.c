/* SPDX-License-Identifier: GPL-2.0-or-later */
/* The Online tab of the settings window (M6.5a). The 2017 window shows only "Coming Soon" (reference
 * tab-online.jpg), and the port has no online play, so this tab is the same panel and stores nothing:
 * the voice chat and UDP values of the 2017 settings file have no effect here. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include "settings_online.h"

#define PAGE_X 24
#define PAGE_Y 150
#define PAGE_W 752
#define PAGE_H 506

static const wchar_t *kOnlineClass = L"PesSettingsOnline";
static HWND g_page;
static HBRUSH g_white;
static HFONT g_big;

static LRESULT CALLBACK OnlinePageProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CTLCOLORSTATIC:
        /* The "Coming Soon" text sits on the white panel. */
        SetBkColor((HDC)wp, RGB(255, 255, 255));
        SetTextColor((HDC)wp, RGB(0, 0, 0));
        return (LRESULT)g_white;
    case WM_DESTROY:
        if (g_white) DeleteObject(g_white);
        if (g_big) DeleteObject(g_big);
        g_white = NULL;
        g_big = NULL;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void RegisterOnlineClass(void) {
    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    if (GetClassInfoExW(GetModuleHandleW(NULL), kOnlineClass, &wc)) return;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = OnlinePageProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = kOnlineClass;
    RegisterClassExW(&wc);
}

void SettingsOnlineCreate(HWND parent, HFONT font) {
    (void)font;
    RegisterOnlineClass();
    g_page = CreateWindowExW(0, kOnlineClass, L"", WS_CHILD | WS_CLIPCHILDREN, PAGE_X, PAGE_Y, PAGE_W, PAGE_H,
                             parent, NULL, GetModuleHandleW(NULL), NULL);
    g_white = CreateSolidBrush(RGB(255, 255, 255));
    /* A larger face for the one line of text, as the reference shows. */
    g_big = CreateFontW(-26, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    /* The white panel covers the page above the bottom strip, as in the reference. */
    CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_WHITERECT, 0, 0, PAGE_W, 430, g_page, NULL,
                    GetModuleHandleW(NULL), NULL);
    HWND text = CreateWindowExW(0, L"STATIC", L"Coming Soon", WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
                                0, 0, PAGE_W, 430, g_page, NULL, GetModuleHandleW(NULL), NULL);
    SendMessageW(text, WM_SETFONT, (WPARAM)g_big, TRUE);

    ShowWindow(g_page, SW_HIDE);
}

void SettingsOnlineShow(int visible) {
    if (g_page) ShowWindow(g_page, visible ? SW_SHOW : SW_HIDE);
}
