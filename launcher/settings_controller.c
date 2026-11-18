/* SPDX-License-Identifier: GPL-2.0-or-later */
/* The Controller tab of the settings window (M6.5a): the gamepad bindings around the PS4 controller
 * picture. Clicking a binding button waits for a button press on the controller (polled through
 * XInput for up to eight seconds); Esc cancels. The analog stick directions are shown, not bindable.
 * The bindings and their rules live in settings_pad (tested on every platform). The layout follows the
 * Keyboard tab and is estimated from the reference screenshots; it needs a visual check. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "settings_controller.h"
#include "settings_keys.h"
#include "settings_pad.h"
#include "settings_ini.h"

#define PAGE_X 24
#define PAGE_Y 150
#define PAGE_W 752
#define PAGE_H 506
#define IDB_CONTROLLER_PAD 202
#define IDB_ICON_BASE 210  /* icon of slot s is IDB_ICON_BASE + s */
#define ID_BIND_BASE 700   /* button for slot s is ID_BIND_BASE + s */
#define ID_REVERT 800
#define CAPTURE_TIMER 7
#define CAPTURE_MS 8000

/* XInput's state layout (XINPUT_STATE), loaded from the system library at run time. */
typedef struct { WORD buttons; BYTE left_trigger, right_trigger; SHORT lx, ly, rx, ry; } PadState;
typedef struct { DWORD packet; PadState pad; } XState;
typedef DWORD (WINAPI *GetStateFn)(DWORD, XState *);

static const wchar_t *kControllerClass = L"PesSettingsController";
static HWND g_page, g_binds[PES_PAD_SLOTS];
static char g_pads[PES_PAD_SLOTS][PES_PAD_NAME_MAX];
static int g_capture = -1;
static DWORD g_capture_deadline;
static XState g_baseline;
static GetStateFn g_get_state;
static int g_xinput_tried;

static GetStateFn LoadXInput(void) {
    if (g_xinput_tried) return g_get_state;
    g_xinput_tried = 1;
    static const wchar_t *names[] = {L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll"};
    for (int i = 0; i < 3 && !g_get_state; ++i) {
        HMODULE lib = LoadLibraryW(names[i]);
        if (lib) g_get_state = (GetStateFn)(void *)GetProcAddress(lib, "XInputGetState");
    }
    return g_get_state;
}

/* Reads controller 0; returns 0 if none is connected. */
static int ReadPad(XState *state) {
    GetStateFn get = LoadXInput();
    if (!get) return 0;
    return get(0, state) == 0;
}

static void SetText(int slot) {
    wchar_t wide[32];
    char label[32];
    if (g_capture == slot) {
        wcscpy(wide, L"Press a button...");
    } else if (!PesPadBindable(slot)) {
        wcscpy(wide, L"Stick (analog)");
    } else {
        PesPadLabel(g_pads[slot], label, sizeof(label));
        swprintf(wide, 32, L"%S", label);
    }
    SetWindowTextW(g_binds[slot], wide);
}

static void RefreshAll(void) {
    for (int s = 0; s < PES_PAD_SLOTS; ++s) SetText(s);
}

static void StopCapture(HWND page) {
    const int previous = g_capture;
    g_capture = -1;
    KillTimer(page, CAPTURE_TIMER);
    if (previous >= 0) SetText(previous);
}

static void StartCapture(HWND page, int slot) {
    XState now;
    if (!ReadPad(&now)) {
        MessageBoxW(page, L"Connect a controller to set this button.", L"Settings", MB_OK | MB_ICONINFORMATION);
        return;
    }
    StopCapture(page);
    g_baseline = now; /* buttons already held do not count */
    g_capture = slot;
    g_capture_deadline = GetTickCount() + CAPTURE_MS;
    SetTimer(page, CAPTURE_TIMER, 30, NULL);
    SetText(slot);
}

static void OnTimer(HWND page) {
    if (g_capture < 0) return;
    if (GetTickCount() > g_capture_deadline) {
        StopCapture(page);
        return;
    }
    XState now;
    if (!ReadPad(&now)) return;
    /* New presses only: a button or a trigger that was not down at the start. */
    const WORD fresh = now.pad.buttons & ~g_baseline.pad.buttons;
    const unsigned left = now.pad.left_trigger > 127 && g_baseline.pad.left_trigger <= 127 ? 255 : 0;
    const unsigned right = now.pad.right_trigger > 127 && g_baseline.pad.right_trigger <= 127 ? 255 : 0;
    const char *name = PesPadNameFromXInput(fresh, left, right);
    if (name[0]) {
        PesPadBind(g_pads, g_capture, name);
        StopCapture(page);
        RefreshAll();
    }
}

static LRESULT CALLBACK ControllerPageProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_TIMER:
        if (wp == CAPTURE_TIMER) OnTimer(hwnd);
        return 0;
    case WM_COMMAND: {
        const int id = LOWORD(wp);
        if (HIWORD(wp) != BN_CLICKED) break;
        if (id >= ID_BIND_BASE && id < ID_BIND_BASE + PES_PAD_SLOTS) {
            const int slot = id - ID_BIND_BASE;
            if (PesPadBindable(slot)) StartCapture(hwnd, slot);
            return 0;
        }
        if (id == ID_REVERT) {
            StopCapture(hwnd);
            PesPadDefaults(g_pads);
            RefreshAll();
            return 0;
        }
        break;
    }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void RegisterControllerClass(void) {
    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    if (GetClassInfoExW(GetModuleHandleW(NULL), kControllerClass, &wc)) return;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = ControllerPageProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = kControllerClass;
    RegisterClassExW(&wc);
}

static HWND Make(HWND parent, const wchar_t *cls, const wchar_t *text, DWORD style, int x, int y, int w, int h, int id) {
    return CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, parent,
                           (HMENU)(INT_PTR)id, GetModuleHandleW(NULL), NULL);
}

/* One column: the slot's icon and the binding button (stick directions are disabled). */
static void Column(HWND p, const int slots[12], int chip_x, int button_x) {
    for (int i = 0; i < 12; ++i) {
        const int s = slots[i];
        const int y = 115 + 33 * i;
        HWND icon = Make(p, L"STATIC", L"", SS_BITMAP, chip_x + 17, y + 1, 24, 24, 0);
        HBITMAP bmp = LoadBitmapW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDB_ICON_BASE + s));
        if (bmp) SendMessageW(icon, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bmp);
        g_binds[s] = Make(p, L"BUTTON", L"", BS_PUSHBUTTON | WS_TABSTOP, button_x, y, 130, 26, ID_BIND_BASE + s);
        if (!PesPadBindable(s)) EnableWindow(g_binds[s], FALSE);
    }
}

/* Same two columns as the keyboard tab, from the same slot order. */
static const int kLeft[12] = {4, 6, 12, 14, 13, 15, 16, 18, 17, 19, 10, 9};
static const int kRight[12] = {5, 7, 3, 0, 1, 2, 20, 22, 21, 23, 11, 8};

void SettingsControllerCreate(HWND parent, HFONT font) {
    RegisterControllerClass();
    g_page = CreateWindowExW(0, kControllerClass, L"", WS_CHILD | WS_CLIPCHILDREN, PAGE_X, PAGE_Y, PAGE_W, PAGE_H,
                             parent, NULL, GetModuleHandleW(NULL), NULL);
    HWND p = g_page;
    /* Controller 1 (the first connected pad) and the device line, as the original shows them. */
    Make(p, L"STATIC", L"Controller", SS_LEFT, 0, 34, 90, 20, 0);
    HWND controller = Make(p, L"COMBOBOX", L"Controller 1", CBS_DROPDOWNLIST | WS_TABSTOP, 100, 30, 252, 120, 0);
    SendMessageW(controller, CB_ADDSTRING, 0, (LPARAM)L"Controller 1");
    SendMessageW(controller, CB_SETCURSEL, 0, 0);
    Make(p, L"BUTTON", L"DirectInput", BS_AUTORADIOBUTTON, 446, 36, 120, 22, 0);
    Make(p, L"BUTTON", L"XInput", BS_AUTORADIOBUTTON | WS_GROUP, 594, 36, 120, 22, 0);
    Make(p, L"STATIC", L"Device", SS_LEFT, 0, 72, 90, 20, 0);
    HWND device = Make(p, L"COMBOBOX", L"XInput controller", CBS_DROPDOWNLIST | WS_TABSTOP, 100, 68, 629, 120, 0);
    SendMessageW(device, CB_ADDSTRING, 0, (LPARAM)L"XInput controller");
    SendMessageW(device, CB_SETCURSEL, 0, 0);

    Column(p, kLeft, 21, 86);
    Column(p, kRight, 548, 614);
    HWND picture = Make(p, L"STATIC", L"", SS_BITMAP, 258, 109, 260, 325, 0);
    HBITMAP bmp = LoadBitmapW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDB_CONTROLLER_PAD));
    if (bmp) SendMessageW(picture, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bmp);
    Make(p, L"BUTTON", L"Revert to Default Settings", BS_PUSHBUTTON | WS_TABSTOP, 232, 448, 313, 30, ID_REVERT);

    for (HWND c = GetWindow(p, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) SendMessageW(c, WM_SETFONT, (WPARAM)font, TRUE);
    PesPadDefaults(g_pads);
    {
        int ignored[PES_PAD_SLOTS]; /* the keyboard part belongs to the Keyboard tab */
        PesIniLoadBindings(PesConfigPath(), ignored, g_pads); /* the saved gamepad bindings, if the file has any */
    }
    RefreshAll();
    ShowWindow(g_page, SW_HIDE);
}

void SettingsControllerShow(int visible) {
    if (g_page) ShowWindow(g_page, visible ? SW_SHOW : SW_HIDE);
    if (!visible && g_capture >= 0) StopCapture(g_page);
}

BOOL SettingsControllerFilter(const MSG *msg) {
    if (g_capture < 0) return FALSE;
    if ((msg->message == WM_KEYDOWN || msg->message == WM_SYSKEYDOWN) && msg->wParam == VK_ESCAPE) {
        StopCapture(g_page);
        return TRUE;
    }
    return FALSE;
}

void SettingsControllerGet(char pads[PES_PAD_SLOTS][PES_PAD_NAME_MAX]) {
    memcpy(pads, g_pads, sizeof(g_pads));
}

void SettingsControllerSet(const char pads[PES_PAD_SLOTS][PES_PAD_NAME_MAX]) {
    StopCapture(g_page);
    memcpy(g_pads, pads, sizeof(g_pads));
    RefreshAll();
}
