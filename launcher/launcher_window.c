/* SPDX-License-Identifier: GPL-2.0-or-later */
/* The launcher (pes17launcher.exe): a fixed window with four tiles, Install, Play, Settings and
 * Exit, drawn on the settings window's artwork. Each tile is a picture under a black see-through
 * overlay with its name in the middle; the tile under the mouse (or chosen with the arrow keys) is
 * brighter and framed in white.
 *
 * Install picks the folder of the PS4 game (it must hold eboot.bin), saves it as game_dir in the
 * settings file and prepares the game once: three Python 3 scripts (scripts/) turn the game folder
 * into the files the loader needs, in game-data/ next to the launcher, logged in
 * game-data/prepare.log. Play starts the game with that folder (it prepares first if that has not
 * been done) and closes the launcher. Settings opens pes17settings.exe. Exit closes the launcher.
 * The programs find their runtime libraries (SDL3 and the rest) next to them or on PATH. */
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
#define WM_PREPARE_STEP (WM_APP + 1)
#define WM_PREPARE_DONE (WM_APP + 2)

enum { TILE_INSTALL, TILE_PLAY, TILE_SETTINGS, TILE_EXIT };
static const wchar_t *kTileNames[TILE_COUNT] = {L"INSTALL", L"PLAY", L"SETTINGS", L"EXIT"};

static HBITMAP g_bg, g_tiles[TILE_COUNT];
static HFONT g_title_font, g_sub_font, g_tile_font, g_note_font;
static int g_hover = -1, g_selected = -1; /* mouse, keyboard */
static char g_game_dir[1024];
static char g_job_game_dir[1024];      /* the game folder the running preparation works on (a private copy) */
static char g_root[MAX_PATH * 3];     /* where scripts/ and src/ are (UTF-8, forward slashes) */
static char g_data_dir[MAX_PATH * 3]; /* game-data next to the launcher */
static wchar_t g_exe_dir[MAX_PATH];
static volatile LONG g_busy, g_step;  /* preparation running, and its current step */
static int g_play_after;              /* start the game when the preparation finishes */
static wchar_t g_fail_text[700];

/* ---- helpers ------------------------------------------------------------------------------- */
static void Narrow(const wchar_t *wide, char *out, size_t cap, int slashes) {
    if (!WideCharToMultiByte(CP_UTF8, 0, wide, -1, out, (int)cap, NULL, NULL)) out[0] = '\0';
    if (slashes) for (char *c = out; *c; ++c) if (*c == '\\') *c = '/';
}

static int Widen(const char *utf8, wchar_t *out, int cap) {
    return MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out, cap) != 0;
}

static int FileExistsW(const wchar_t *path) {
    return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}

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

/* ---- painting ------------------------------------------------------------------------------ */
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
    const int busy = g_busy != 0;

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

    if (busy) {
        wchar_t status[200];
        const int step = (int)g_step;
        wchar_t name[80];
        Widen(PesLauncherStepName(step), name, 80);
        swprintf(status, 200, L"Preparing the game, step %d of %d: %ls. This runs once and can take a few minutes.",
                 step + 1, PES_LAUNCHER_STEPS, name);
        RECT r = {300, 100, CLIENT_W - 30, 136};
        DrawText2(dc, status, g_note_font, RGB(235, 205, 120), r, DT_RIGHT | DT_WORDBREAK);
    }

    /* A 1x1 black bitmap stretched over each tile with constant alpha: the fading overlay. */
    HDC black_dc = CreateCompatibleDC(screen);
    HBITMAP black = CreateCompatibleBitmap(screen, 1, 1);
    HGDIOBJ o_black = SelectObject(black_dc, black);
    SetPixel(black_dc, 0, 0, RGB(0, 0, 0));

    for (int i = 0; i < TILE_COUNT; ++i) {
        const RECT r = TileRect(i);
        const int active = !busy && (i == g_hover || (g_hover < 0 && i == g_selected));
        const int usable = !busy && (i != TILE_PLAY || PlayReady());
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
        if (i == TILE_PLAY && !busy) {
            const wchar_t *note = !PlayReady() ? L"Install first: choose the game folder"
                                  : !PesLauncherPrepared(g_data_dir, g_game_dir) ? L"First start: the game is prepared once"
                                                                     : NULL;
            if (note) {
                RECT n = {r.left, r.bottom - 40, r.right, r.bottom - 10};
                DrawText2(dc, note, g_note_font, RGB(170, 170, 170), n, DT_CENTER | DT_SINGLELINE);
            }
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

/* ---- starting programs --------------------------------------------------------------------- */
/* Runs cmd (a command line, UTF-16) without a window and waits up to timeout_ms. Output goes to
 * log when it is not NULL. Returns 1 if the process ran and exited, with its code in *code. */
static int RunHidden(const wchar_t *cmd, const wchar_t *cwd, HANDLE log, DWORD timeout_ms, DWORD *code) {
    wchar_t line[8192];
    wcsncpy(line, cmd, 8191);
    line[8191] = 0;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    if (log) {
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = log;
        si.hStdError = log;
        si.hStdInput = INVALID_HANDLE_VALUE;
    }
    if (!CreateProcessW(NULL, line, NULL, NULL, log != NULL, CREATE_NO_WINDOW, NULL, cwd, &si, &pi)) return 0;
    const DWORD waited = WaitForSingleObject(pi.hProcess, timeout_ms);
    int ok = 0;
    if (waited == WAIT_OBJECT_0) ok = GetExitCodeProcess(pi.hProcess, code) != 0;
    else TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return ok;
}

/* Starts a program from the launcher's folder with UTF-8 arguments (each already quoted). */
static int Start(HWND hwnd, const wchar_t *exe, const char *args) {
    wchar_t path[MAX_PATH + 40], wide_args[4096], cmd[8192];
    swprintf(path, MAX_PATH + 40, L"%ls\\%ls", g_exe_dir, exe);
    if (!args || !Widen(args, wide_args, 4096)) wide_args[0] = 0;
    swprintf(cmd, 8192, L"\"%ls\"%ls%ls", path, wide_args[0] ? L" " : L"", wide_args);
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    if (!CreateProcessW(path, cmd, NULL, NULL, FALSE, 0, NULL, g_exe_dir, &si, &pi)) {
        wchar_t msg[600];
        swprintf(msg, 600, L"Could not start %ls (error %lu).\nIts runtime libraries (SDL3 and the rest) must be next to it or on PATH.",
                 exe, GetLastError());
        MessageBoxW(hwnd, msg, L"Launcher", MB_OK | MB_ICONERROR);
        return 0;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 1;
}

/* ---- the one-time preparation -------------------------------------------------------------- */
/* The first of python, python3 and py that runs as Python 3. */
static const wchar_t *FindPython(void) {
    static const wchar_t *kNames[] = {L"python", L"python3", L"py"};
    for (int i = 0; i < 3; ++i) {
        wchar_t cmd[200];
        DWORD code = 1;
        swprintf(cmd, 200, L"%ls -c \"import sys; sys.exit(0 if sys.version_info[0] >= 3 else 1)\"", kNames[i]);
        if (RunHidden(cmd, NULL, NULL, 20000, &code) && code == 0) return kNames[i];
    }
    return NULL;
}

static DWORD WINAPI PrepareThread(LPVOID param) {
    HWND hwnd = (HWND)param;
    const wchar_t *python = FindPython();
    if (!python) {
        wcscpy(g_fail_text, L"Python 3 is needed for the one-time preparation of the game. Install it (python.org), "
                            L"make sure it is on PATH, and try again.");
        PostMessageW(hwnd, WM_PREPARE_DONE, 0, 0);
        return 0;
    }
    wchar_t wide_data[MAX_PATH * 2], wide_root[MAX_PATH * 2], log_path[MAX_PATH * 2];
    Widen(g_data_dir, wide_data, MAX_PATH * 2);
    Widen(g_root, wide_root, MAX_PATH * 2);
    SHCreateDirectoryExW(NULL, wide_data, NULL);
    swprintf(log_path, MAX_PATH * 2, L"%ls\\prepare.log", wide_data);
    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
    HANDLE log = CreateFileW(log_path, GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    for (int step = 0; step < PES_LAUNCHER_STEPS; ++step) {
        InterlockedExchange(&g_step, step);
        PostMessageW(hwnd, WM_PREPARE_STEP, 0, 0);
        char args[4096];
        wchar_t wide_args[4096], cmd[8192];
        DWORD code = 1;
        if (!PesLauncherStepArgs(step, args, sizeof(args), g_root, g_data_dir, g_job_game_dir) || !Widen(args, wide_args, 4096)) {
            swprintf(g_fail_text, 700, L"The preparation command is too long (step %d).", step + 1);
            break;
        }
        swprintf(cmd, 8192, L"%ls%ls", python, wide_args);
        const int ran = RunHidden(cmd, wide_root, log, 45 * 60 * 1000, &code);
        if (!ran || code != 0) {
            swprintf(g_fail_text, 700, L"Preparing the game failed at step %d of %d (%ls).\nThe details are in:\n%ls",
                     step + 1, PES_LAUNCHER_STEPS, ran ? L"the script reported an error" : L"it did not finish", log_path);
            if (log) CloseHandle(log);
            PostMessageW(hwnd, WM_PREPARE_DONE, 0, 0);
            return 0;
        }
        g_fail_text[0] = 0;
    }
    if (log) CloseHandle(log);
    /* Only a preparation that ran every step is stamped as current for this game folder. */
    const int ok = g_fail_text[0] == 0 && PesLauncherWriteStamp(g_data_dir, g_job_game_dir) &&
                   PesLauncherPrepared(g_data_dir, g_job_game_dir);
    PostMessageW(hwnd, WM_PREPARE_DONE, ok, 0);
    return 0;
}

static void StartPrepare(HWND hwnd, int play_after) {
    if (g_busy) return;
    g_fail_text[0] = 0;
    g_play_after = play_after;
    snprintf(g_job_game_dir, sizeof(g_job_game_dir), "%s", g_game_dir); /* the worker never reads g_game_dir */
    PesLauncherClearStamp(g_data_dir);                                     /* not current until it succeeds */
    InterlockedExchange(&g_busy, 1);
    InterlockedExchange(&g_step, 0);
    HANDLE t = CreateThread(NULL, 0, PrepareThread, hwnd, 0, NULL);
    if (!t) {
        InterlockedExchange(&g_busy, 0);
        MessageBoxW(hwnd, L"Could not start the preparation.", L"Launcher", MB_OK | MB_ICONERROR);
        return;
    }
    CloseHandle(t);
    InvalidateRect(hwnd, NULL, FALSE);
}

/* ---- the four tiles ------------------------------------------------------------------------ */
static void Play(HWND hwnd) {
    if (!PlayReady()) {
        MessageBoxW(hwnd, L"Choose the game folder first (Install).", L"Play", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (!PesLauncherPrepared(g_data_dir, g_game_dir)) {
        if (MessageBoxW(hwnd, L"The game has to be prepared once before the first start. It can take a few minutes.\n\nPrepare it now?",
                        L"Play", MB_YESNO | MB_ICONQUESTION) == IDYES) StartPrepare(hwnd, 1);
        return;
    }
    char args[4096];
    if (!PesLauncherPlayArgs(args, sizeof(args), g_data_dir, g_game_dir)) {
        MessageBoxW(hwnd, L"The prepared game files are incomplete. Use Install again to prepare them.", L"Play",
                    MB_OK | MB_ICONERROR);
        return;
    }
    if (Start(hwnd, L"pes17recomp.exe", args)) DestroyWindow(hwnd);
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
    Narrow(wide, dir, sizeof(dir), 0);
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
    StartPrepare(hwnd, 0);
}

static void Activate(HWND hwnd, int tile) {
    if (g_busy) return;
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
    case WM_PREPARE_STEP:
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_PREPARE_DONE:
        InterlockedExchange(&g_busy, 0);
        InvalidateRect(hwnd, NULL, FALSE);
        if (!wp) {
            MessageBoxW(hwnd, g_fail_text[0] ? g_fail_text : L"Preparing the game failed.", L"Install", MB_OK | MB_ICONERROR);
        } else if (g_play_after) {
            Play(hwnd);
        } else {
            MessageBoxW(hwnd, L"The game is ready. Press Play.", L"Install", MB_OK | MB_ICONINFORMATION);
        }
        return 0;
    case WM_CLOSE:
        if (g_busy) {
            MessageBoxW(hwnd, L"The game is being prepared. Please wait until it has finished.", L"Launcher",
                        MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        break;
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
        if (LOWORD(lp) == HTCLIENT && g_hover >= 0 && !g_busy) {
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
        else if (wp == VK_ESCAPE) { if (!g_busy) DestroyWindow(hwnd); return 0; }
        else break;
        g_selected = s;
        g_hover = -1;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_ACTIVATE:
        if (!g_busy) LoadGameDir(); /* Settings or a previous Install may have changed it */
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

/* Finds the launcher's folder and where scripts/ lives: next to the launcher, or one folder up (the
 * build puts the programs in out/). */
static void FindFolders(void) {
    GetModuleFileNameW(NULL, g_exe_dir, MAX_PATH);
    wchar_t *cut = wcsrchr(g_exe_dir, L'\\');
    if (cut) *cut = 0;
    wchar_t probe[MAX_PATH + 40], root[MAX_PATH];
    wcscpy(root, g_exe_dir);
    swprintf(probe, MAX_PATH + 40, L"%ls\\scripts\\prepare.py", root);
    if (!FileExistsW(probe)) {
        wchar_t *up = wcsrchr(root, L'\\');
        if (up) *up = 0;
    }
    Narrow(root, g_root, sizeof(g_root), 1);
    wchar_t data[MAX_PATH + 20];
    swprintf(data, MAX_PATH + 20, L"%ls\\game-data", g_exe_dir);
    Narrow(data, g_data_dir, sizeof(g_data_dir), 1);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, LPWSTR cmdline, int show) {
    (void)prev;
    (void)cmdline;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED); /* the folder picker */
    FindFolders();
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
