/* SPDX-License-Identifier: GPL-2.0-or-later */
/* The launcher (pes17launcher.exe): a fixed window with four tiles, Install, Play, Settings and
 * Exit, drawn on the settings window's artwork. Each tile is a picture under a black see-through
 * overlay with its name in the middle; the tile under the mouse (or chosen with the arrow keys) is
 * brighter and framed in white. Install picks the folder of the PS4 game (it must hold eboot.bin)
 * and saves it as game_dir in the settings file; Play starts the game with that folder and closes
 * the launcher; Settings opens pes17settings.exe; Exit closes the launcher. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "launcher_game.h"
#include "settings_ini.h"

#define CLIENT_W 800
#define CLIENT_H 560
#define IDB_LAUNCHER_BG 300
#define IDB_TILE_BASE 301 /* Install, Play, Settings, Exit */
#define TILE_COUNT 4
#define TILE_W 340
#define TILE_H 170

enum { TILE_INSTALL, TILE_PLAY, TILE_SETTINGS, TILE_EXIT };
static const wchar_t *kTileNames[TILE_COUNT] = {L"INSTALL", L"PLAY", L"SETTINGS", L"EXIT"};

static HBITMAP g_bg, g_tiles[TILE_COUNT];
static HFONT g_title_font, g_sub_font, g_tile_font, g_note_font;
static int g_hover = -1, g_selected = -1; /* mouse, keyboard */
static char g_game_dir[1024];
static char g_base[MAX_PATH]; /* the launcher's folder, forward slashes */

static RECT TileRect(int i) {
    const int x = 50 + (i % 2) * (TILE_W + 20), y = 140 + (i / 2) * (TILE_H + 20);
    RECT r = {x, y, x + TILE_W, y + TILE_H};
    return r;
}

static int HitTile(int x, int y) {
    for (int i = 0; i < TILE_COUNT; ++i) {
        const RECT r = TileRect(i);
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) return i;
    }
    return -1;
}

static int PlayReady(void) {
    return PesLauncherIsGameDir(g_game_dir);
}

static void LoadGameDir(void) {
    if (!PesIniLoadValue(PesConfigPath(), "game_dir", g_game_dir, sizeof(g_game_dir))) g_game_dir[0] = '\0';
}

static void DrawText2(HDC dc, const wchar_t *text, HFONT font, COLORREF color, RECT r, UINT flags) {
    HGDIOBJ old = SelectObject(dc, font);
    SetTextColor(dc, color);
    DrawTextW(dc, text, -1, &r, flags);
    SelectObject(dc, old);
}

static void FrameRect2(HDC dc, RECT r, int width, COLORREF color) {
    HBRUSH b = CreateSolidBrush(color);
    for (int i = 0; i < width; ++i) {
        RECT f = {r.left + i, r.top + i, r.right - i, r.bottom - i};
        FrameRect(dc, &f, b);
    }
    DeleteObject(b);
}

static void Paint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC screen = BeginPaint(hwnd, &ps);
    HDC dc = CreateCompatibleDC(screen);
    HBITMAP canvas = CreateCompatibleBitmap(screen, CLIENT_W, CLIENT_H);
    HGDIOBJ old_canvas = SelectObject(dc, canvas);
    HDC src = CreateCompatibleDC(screen);

    /* Background artwork. */
    if (g_bg) {
        HGDIOBJ o = SelectObject(src, g_bg);
        BitBlt(dc, 0, 0, CLIENT_W, CLIENT_H, src, 0, 0, SRCCOPY);
        SelectObject(src, o);
    }
    SetBkMode(dc, TRANSPARENT);

    /* The project's own wordmark: "2017" in white and "RECOMP" in red, set solid as one word in a
     * heavy slanted sans (Arial Black, italic; not the game's typeface), a red rule, and
     * "LAUNCHER" spaced out beneath. */
    {
        const COLORREF red = RGB(214, 30, 42);
        HGDIOBJ o = SelectObject(dc, g_title_font);
        SetTextColor(dc, RGB(255, 255, 255));
        TextOutW(dc, 42, 26, L"2017", 4);
        SIZE first;
        GetTextExtentPoint32W(dc, L"2017", 4, &first);
        SetTextColor(dc, red);
        TextOutW(dc, 42 + first.cx, 26, L"RECOMP", 6);
        SIZE second;
        GetTextExtentPoint32W(dc, L"RECOMP", 6, &second);
        SelectObject(dc, o);
        HBRUSH rule = CreateSolidBrush(red);
        RECT bar = {44, 88, 44 + first.cx + second.cx - 6, 91};
        FillRect(dc, &bar, rule);
        DeleteObject(rule);
        o = SelectObject(dc, g_sub_font);
        SetTextColor(dc, RGB(230, 230, 230));
        SetTextCharacterExtra(dc, 9);
        TextOutW(dc, 46, 95, L"LAUNCHER", 8);
        SetTextCharacterExtra(dc, 0);
        SelectObject(dc, o);
    }
    RECT frame = {16, 16, CLIENT_W - 16, CLIENT_H - 16};
    FrameRect2(dc, frame, 3, RGB(255, 255, 255));

    /* A 1x1 black bitmap stretched over each tile with constant alpha: the fading overlay. */
    HDC black_dc = CreateCompatibleDC(screen);
    HBITMAP black = CreateCompatibleBitmap(screen, 1, 1);
    HGDIOBJ o_black = SelectObject(black_dc, black);
    SetPixel(black_dc, 0, 0, RGB(0, 0, 0));

    for (int i = 0; i < TILE_COUNT; ++i) {
        const RECT r = TileRect(i);
        const int active = i == g_hover || (g_hover < 0 && i == g_selected);
        const int usable = i != TILE_PLAY || PlayReady();
        if (g_tiles[i]) {
            HGDIOBJ o = SelectObject(src, g_tiles[i]);
            BitBlt(dc, r.left, r.top, TILE_W, TILE_H, src, 0, 0, SRCCOPY);
            SelectObject(src, o);
        }
        BLENDFUNCTION blend = {AC_SRC_OVER, 0, (BYTE)(!usable ? 200 : active ? 90 : 160), 0};
        AlphaBlend(dc, r.left, r.top, TILE_W, TILE_H, black_dc, 0, 0, 1, 1, blend);
        FrameRect2(dc, r, active ? 3 : 2, active ? RGB(255, 255, 255) : RGB(110, 110, 110));
        const COLORREF text = !usable ? RGB(110, 110, 110) : active ? RGB(255, 255, 255) : RGB(175, 175, 175);
        DrawText2(dc, kTileNames[i], g_tile_font, text, r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (i == TILE_PLAY && !usable) {
            RECT note = {r.left, r.bottom - 40, r.right, r.bottom - 10};
            DrawText2(dc, L"Install first: choose the game folder", g_note_font, RGB(170, 170, 170), note,
                      DT_CENTER | DT_SINGLELINE);
        }
    }

    SelectObject(black_dc, o_black);
    DeleteObject(black);
    DeleteDC(black_dc);
    BitBlt(screen, 0, 0, CLIENT_W, CLIENT_H, dc, 0, 0, SRCCOPY);
    DeleteDC(src);
    SelectObject(dc, old_canvas);
    DeleteObject(canvas);
    DeleteDC(dc);
    EndPaint(hwnd, &ps);
}

/* Starts a program from the launcher's folder. The MSYS2 runtime folder is put in front of PATH
 * when the launcher's folder has no SDL3.dll (a developer build); a packaged build ships it. */
static int Start(HWND hwnd, const wchar_t *exe, const char *args) {
    wchar_t dir[MAX_PATH], path[MAX_PATH], cmd[8192];
    GetModuleFileNameW(NULL, dir, MAX_PATH);
    wchar_t *cut = wcsrchr(dir, L'\\');
    if (cut) *cut = 0;
    swprintf(path, MAX_PATH, L"%ls\\%ls", dir, exe);
    swprintf(cmd, 8192, L"\"%ls\" %S", path, args ? args : "");
    wchar_t sdl[MAX_PATH];
    swprintf(sdl, MAX_PATH, L"%ls\\SDL3.dll", dir);
    if (GetFileAttributesW(sdl) == INVALID_FILE_ATTRIBUTES &&
        GetFileAttributesW(L"C:\\msys64\\clang64\\bin\\SDL3.dll") != INVALID_FILE_ATTRIBUTES) {
        static wchar_t env[32768];
        const DWORD n = GetEnvironmentVariableW(L"PATH", env, 32768);
        if (n < 32000 && !wcsstr(env, L"msys64\\clang64\\bin")) {
            wchar_t next[32768];
            swprintf(next, 32768, L"C:\\msys64\\clang64\\bin;%ls", env);
            SetEnvironmentVariableW(L"PATH", next);
        }
    }
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    if (!CreateProcessW(path, cmd, NULL, NULL, FALSE, 0, NULL, dir, &si, &pi)) {
        wchar_t msg[600];
        swprintf(msg, 600, L"Could not start %ls (error %lu).", exe, GetLastError());
        MessageBoxW(hwnd, msg, L"Launcher", MB_OK | MB_ICONERROR);
        return 0;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 1;
}

static void Install(HWND hwnd) {
    BROWSEINFOW bi;
    memset(&bi, 0, sizeof(bi));
    bi.hwndOwner = hwnd;
    bi.lpszTitle = L"Choose the folder of the PS4 game (the one with eboot.bin, e.g. CUSA04957).";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE list = SHBrowseForFolderW(&bi);
    if (!list) return;
    wchar_t wide[MAX_PATH];
    const BOOL got = SHGetPathFromIDListW(list, wide);
    CoTaskMemFree(list);
    if (!got) return;
    char dir[1024];
    if (!WideCharToMultiByte(CP_UTF8, 0, wide, -1, dir, sizeof(dir), NULL, NULL)) return;
    if (!PesLauncherIsGameDir(dir)) {
        MessageBoxW(hwnd, L"That folder does not hold the game: eboot.bin is missing.\n"
                          L"Choose the game's own folder (for example CUSA04957).",
                    L"Install", MB_OK | MB_ICONWARNING);
        return;
    }
    const char *keys[1] = {"game_dir"};
    const char *values[1] = {dir};
    if (!PesIniSaveValues(PesConfigPath(), keys, values, 1)) {
        MessageBoxW(hwnd, L"The game folder could not be saved.", L"Install", MB_OK | MB_ICONERROR);
        return;
    }
    snprintf(g_game_dir, sizeof(g_game_dir), "%s", dir);
    wchar_t msg[MAX_PATH + 64];
    swprintf(msg, MAX_PATH + 64, L"Game folder set:\n%ls", wide);
    MessageBoxW(hwnd, msg, L"Install", MB_OK | MB_ICONINFORMATION);
    InvalidateRect(hwnd, NULL, FALSE);
}

static void Play(HWND hwnd) {
    if (!PlayReady()) {
        MessageBoxW(hwnd, L"Choose the game folder first (Install).", L"Play", MB_OK | MB_ICONINFORMATION);
        return;
    }
    char args[4096];
    if (!PesLauncherPlayArgs(args, sizeof(args), g_base, g_game_dir)) {
        MessageBoxW(hwnd, L"The game's boot image (verify\\m2b\\boot-linked.bin) is missing next to the launcher.",
                    L"Play", MB_OK | MB_ICONERROR);
        return;
    }
    if (Start(hwnd, L"pes17recomp.exe", args)) DestroyWindow(hwnd);
}

static void Activate(HWND hwnd, int tile) {
    switch (tile) {
    case TILE_INSTALL: Install(hwnd); break;
    case TILE_PLAY: Play(hwnd); break;
    case TILE_SETTINGS: Start(hwnd, L"pes17settings.exe", NULL); break;
    case TILE_EXIT: DestroyWindow(hwnd); break;
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT:
        Paint(hwnd);
        return 0;
    case WM_ERASEBKGND:
        return 1; /* everything is painted in WM_PAINT */
    case WM_MOUSEMOVE: {
        const int hit = HitTile((short)LOWORD(lp), (short)HIWORD(lp));
        if (hit != g_hover) {
            g_hover = hit;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hwnd, 0};
        TrackMouseEvent(&tme);
        return 0;
    }
    case WM_MOUSELEAVE:
        g_hover = -1;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT && g_hover >= 0) {
            SetCursor(LoadCursorW(NULL, IDC_HAND));
            return TRUE;
        }
        break;
    case WM_LBUTTONUP: {
        const int hit = HitTile((short)LOWORD(lp), (short)HIWORD(lp));
        if (hit >= 0) Activate(hwnd, hit);
        return 0;
    }
    case WM_KEYDOWN: {
        int s = g_selected < 0 ? 0 : g_selected;
        if (wp == VK_LEFT || wp == VK_RIGHT) s ^= 1;
        else if (wp == VK_UP || wp == VK_DOWN) s ^= 2;
        else if (wp == VK_RETURN || wp == VK_SPACE) { if (g_selected >= 0) Activate(hwnd, g_selected); return 0; }
        else if (wp == VK_ESCAPE) { DestroyWindow(hwnd); return 0; }
        else break;
        g_selected = s;
        g_hover = -1;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_ACTIVATE:
        LoadGameDir(); /* Settings or a previous Install may have changed it */
        InvalidateRect(hwnd, NULL, FALSE);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* Arial Black for the wordmark and the tiles (the heavy face of the logo); Segoe UI for small text. */
static HFONT FontFace(int height, int weight, BOOL italic, const wchar_t *face) {
    HFONT f = CreateFontW(-height, 0, 0, 0, weight, italic, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                          CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
    if (f) return f;
    return CreateFontW(-height, 0, 0, 0, weight, italic, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, LPWSTR cmdline, int show) {
    (void)prev;
    (void)cmdline;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED); /* the folder picker */
    {
        wchar_t wide[MAX_PATH];
        GetModuleFileNameW(NULL, wide, MAX_PATH);
        wchar_t *cut = wcsrchr(wide, L'\\');
        if (cut) *cut = 0;
        for (wchar_t *c = wide; *c; ++c) if (*c == L'\\') *c = L'/';
        WideCharToMultiByte(CP_UTF8, 0, wide, -1, g_base, sizeof(g_base), NULL, NULL);
    }
    LoadGameDir();
    g_bg = LoadBitmapW(inst, MAKEINTRESOURCEW(IDB_LAUNCHER_BG));
    for (int i = 0; i < TILE_COUNT; ++i) g_tiles[i] = LoadBitmapW(inst, MAKEINTRESOURCEW(IDB_TILE_BASE + i));
    g_title_font = FontFace(58, FW_BLACK, TRUE, L"Arial Black");
    g_sub_font = FontFace(15, FW_SEMIBOLD, FALSE, L"Segoe UI");
    g_tile_font = FontFace(34, FW_BLACK, TRUE, L"Arial Black");
    g_note_font = FontFace(15, FW_NORMAL, FALSE, L"Segoe UI");

    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = L"Pes17Launcher";
    RegisterClassExW(&wc);

    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT frame = {0, 0, CLIENT_W, CLIENT_H};
    AdjustWindowRect(&frame, style, FALSE);
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"2017RECOMP Launcher", style, CW_USEDEFAULT, CW_USEDEFAULT,
                                frame.right - frame.left, frame.bottom - frame.top, NULL, NULL, inst, NULL);
    if (!hwnd) return 1;
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);
    MSG m;
    while (GetMessageW(&m, NULL, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    CoUninitialize();
    return (int)m.wParam;
}
