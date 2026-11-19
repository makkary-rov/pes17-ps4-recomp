/* SPDX-License-Identifier: GPL-2.0-or-later */
/* The Keyboard tab of the settings window (M6.5a): the 24 bindings around the controller picture.
 * Clicking a binding button waits for a key; the next key pressed becomes the binding (Esc cancels).
 * The bindings and their rules live in settings_keys (tested on every platform). Layout is estimated and
 * needs a visual check. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "settings_keyboard.h"
#include "settings_keys.h"
#include "settings_ini.h"

#define PAGE_X 24
#define PAGE_Y 150
#define PAGE_W 752
#define PAGE_H 506
#define IDB_CONTROLLER_KB 200
#define IDB_ICON_BASE 210  /* icon of slot s is IDB_ICON_BASE + s (launcher/assets/icon_NN.bmp) */
#define ID_BIND_BASE 500  /* button for slot s is ID_BIND_BASE + s */
#define ID_REVERT 600

static const wchar_t *kKeyboardClass = L"PesSettingsKeyboard";
static HWND g_page, g_binds[PES_PAD_SLOTS];
static int g_keys[PES_PAD_SLOTS];
static int g_capture = -1; /* slot waiting for a key, or -1 */

/* Left column (top to bottom) and right column, by slot, as in the reference screenshot. */
static const int kLeft[12] = {4, 6, 12, 14, 13, 15, 16, 18, 17, 19, 10, 9};   /* LT LB Up Left Down Right LS... LS press Back */
static const int kRight[12] = {5, 7, 3, 0, 1, 2, 20, 22, 21, 23, 11, 8};     /* RT RB Y X A B RS... RS press Start */

static void SetText(int slot) {
    char name[32];
    wchar_t wide[32];
    if (g_capture == slot) {
        wcscpy(wide, L"Press a key...");
    } else {
        PesKeyName(g_keys[slot], name, sizeof(name));
        swprintf(wide, 32, L"%S", name);
    }
    SetWindowTextW(g_binds[slot], wide);
}

static void RefreshAll(void) {
    for (int s = 0; s < PES_PAD_SLOTS; ++s) SetText(s);
}

static void StartCapture(int slot) {
    const int previous = g_capture;
    g_capture = slot;
    if (previous >= 0) SetText(previous);
    SetText(slot);
    SetFocus(g_binds[slot]);
}

static void StopCapture(void) {
    const int previous = g_capture;
    g_capture = -1;
    if (previous >= 0) SetText(previous);
}

static LRESULT CALLBACK KeyboardPageProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_COMMAND: {
        const int id = LOWORD(wp);
        if (HIWORD(wp) != BN_CLICKED) break;
        if (id >= ID_BIND_BASE && id < ID_BIND_BASE + PES_PAD_SLOTS) {
            StartCapture(id - ID_BIND_BASE);
            return 0;
        }
        if (id == ID_REVERT) {
            StopCapture();
            PesKeyboardDefaults(g_keys);
            RefreshAll();
            return 0;
        }
        break;
    }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void RegisterKeyboardClass(void) {
    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    if (GetClassInfoExW(GetModuleHandleW(NULL), kKeyboardClass, &wc)) return;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = KeyboardPageProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = kKeyboardClass;
    RegisterClassExW(&wc);
}

static HWND Make(HWND parent, const wchar_t *cls, const wchar_t *text, DWORD style, int x, int y, int w, int h, int id) {
    return CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, parent,
                           (HMENU)(INT_PTR)id, GetModuleHandleW(NULL), NULL);
}

/* One column of rows: a label chip (the slot's name) and the key button. */
static void Column(HWND p, const int slots[12], int chip_x, int button_x) {
    for (int i = 0; i < 12; ++i) {
        const int s = slots[i];
        const int y = 115 + 33 * i;
        /* The button's icon (40 x 26, tools/art/make_settings_art.py), centred in its column. */
        HWND icon = Make(p, L"STATIC", L"", SS_BITMAP, chip_x + 9, y, 40, 26, 0);
        HBITMAP bmp = LoadBitmapW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDB_ICON_BASE + s));
        if (bmp) SendMessageW(icon, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bmp);
        g_binds[s] = Make(p, L"BUTTON", L"", BS_PUSHBUTTON | WS_TABSTOP, button_x, y, 130, 26, ID_BIND_BASE + s);
    }
}

void SettingsKeyboardCreate(HWND parent, HFONT font) {
    RegisterKeyboardClass();
    g_page = CreateWindowExW(0, kKeyboardClass, L"", WS_CHILD | WS_CLIPCHILDREN, PAGE_X, PAGE_Y, PAGE_W, PAGE_H,
                             parent, NULL, GetModuleHandleW(NULL), NULL);
    HWND p = g_page;
    /* Top row: the controller and device choices. For the keyboard both are disabled, as in the original. */
    Make(p, L"STATIC", L"Controller", SS_LEFT, 0, 34, 90, 20, 0);
    HWND controller = Make(p, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 100, 30, 252, 120, 0);
    EnableWindow(controller, FALSE);
    Make(p, L"BUTTON", L"DirectInput", BS_AUTORADIOBUTTON, 446, 36, 120, 22, 0);
    Make(p, L"BUTTON", L"XInput", BS_AUTORADIOBUTTON, 594, 36, 120, 22, 0);
    Make(p, L"STATIC", L"Device", SS_LEFT, 0, 72, 90, 20, 0);
    HWND device = Make(p, L"COMBOBOX", L"Keyboard", CBS_DROPDOWNLIST | WS_TABSTOP, 100, 68, 629, 120, 0);
    SendMessageW(device, CB_ADDSTRING, 0, (LPARAM)L"Keyboard");
    SendMessageW(device, CB_SETCURSEL, 0, 0);
    EnableWindow(device, FALSE);

    /* The gamepad picture (tools/art/cut_gamepad.py), with the two columns of bindings around it. */
    Column(p, kLeft, 21, 86);
    Column(p, kRight, 548, 614);
    HWND picture = Make(p, L"STATIC", L"", SS_BITMAP, 258, 109, 260, 325, 0);
    HBITMAP bmp = LoadBitmapW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDB_CONTROLLER_KB));
    if (bmp) SendMessageW(picture, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bmp);
    Make(p, L"BUTTON", L"Revert to Default Settings", BS_PUSHBUTTON | WS_TABSTOP, 232, 448, 313, 30, ID_REVERT);

    for (HWND c = GetWindow(p, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) SendMessageW(c, WM_SETFONT, (WPARAM)font, TRUE);
    PesKeyboardDefaults(g_keys);
    {
        char ignored[PES_PAD_SLOTS][PES_PAD_NAME_MAX]; /* the gamepad part belongs to the Controller tab */
        PesIniLoadBindings(PesConfigPath(), g_keys, ignored);
    }
    RefreshAll();
    ShowWindow(g_page, SW_HIDE);
}

void SettingsKeyboardShow(int visible) {
    if (g_page) ShowWindow(g_page, visible ? SW_SHOW : SW_HIDE);
    if (!visible) StopCapture();
}

BOOL SettingsKeyboardFilter(const MSG *msg) {
    if (g_capture < 0) return FALSE;
    if (msg->message == WM_KEYDOWN || msg->message == WM_SYSKEYDOWN) {
        const int key = (int)msg->wParam;
        if (key == VK_ESCAPE) {
            StopCapture();
        } else {
            PesKeyboardBind(g_keys, g_capture, key);
            g_capture = -1;
            RefreshAll();
        }
        return TRUE;
    }
    return FALSE;
}

void SettingsKeyboardGet(int keys[PES_PAD_SLOTS]) {
    memcpy(keys, g_keys, sizeof(g_keys));
}

void SettingsKeyboardSet(const int keys[PES_PAD_SLOTS]) {
    StopCapture();
    memcpy(g_keys, keys, sizeof(g_keys));
    RefreshAll();
}
