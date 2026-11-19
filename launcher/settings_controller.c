/* SPDX-License-Identifier: GPL-2.0-or-later */
/* The Controller tab of the settings window (M6.5a): the gamepad bindings around the PS4 controller
 * picture. XInput (the default) keeps the standard mapping; DirectInput lets each button be set.
 * Clicking a binding button waits up to eight seconds for a press on the first gamepad, read through
 * SDL with the same backend the game will use (pad_input: XInput, or DirectInput), so the captured
 * name is the SDL gamepad name the game reads. Esc cancels. The analog stick directions are shown,
 * not bindable. The bindings and their rules live in settings_pad (tested on every platform). */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <SDL3/SDL.h>
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
#define ID_PAD_DIRECT 900
#define ID_PAD_XINPUT 901
#define ID_DEVICE 902
#define CAPTURE_TIMER 7
#define CAPTURE_MS 8000
#define TRIGGER_DOWN 16000 /* of 32767: a trigger pulled past half counts as a press */

/* Pressed controls: one bit per SDL gamepad button, then the two triggers. */
typedef unsigned long long PadBits;
#define BIT_LEFT_TRIGGER ((PadBits)1 << SDL_GAMEPAD_BUTTON_COUNT)
#define BIT_RIGHT_TRIGGER ((PadBits)1 << (SDL_GAMEPAD_BUTTON_COUNT + 1))

static const wchar_t *kControllerClass = L"PesSettingsController";
static HWND g_page, g_binds[PES_PAD_SLOTS], g_direct_radio, g_xinput_radio, g_device;
static char g_pads[PES_PAD_SLOTS][PES_PAD_NAME_MAX];
static int g_method = PES_PAD_XINPUT; /* XInput is the default: the fixed standard mapping */
static int g_capture = -1;
static DWORD g_capture_deadline;
static PadBits g_baseline;
static int g_sdl_backend = -1; /* the backend SDL's gamepad subsystem was started with, -1 none */
static SDL_Gamepad *g_gamepad;

/* Starts (or restarts) SDL's gamepads with the backend of the chosen method, as the game does. */
static void StartPads(void) {
    if (g_sdl_backend == g_method) return;
    if (g_gamepad) { SDL_CloseGamepad(g_gamepad); g_gamepad = NULL; }
    if (g_sdl_backend >= 0) SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    const int direct = g_method == PES_PAD_DIRECTINPUT;
    SDL_SetHint(SDL_HINT_XINPUT_ENABLED, direct ? "0" : "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT, direct ? "0" : "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_DIRECTINPUT, direct ? "1" : "0");
    g_sdl_backend = SDL_InitSubSystem(SDL_INIT_GAMEPAD) ? g_method : -1;
}

/* The first connected gamepad, opened on demand; NULL if none. */
static SDL_Gamepad *Pad(void) {
    StartPads();
    if (g_sdl_backend < 0) return NULL;
    SDL_UpdateGamepads();
    if (g_gamepad && SDL_GamepadConnected(g_gamepad)) return g_gamepad;
    if (g_gamepad) { SDL_CloseGamepad(g_gamepad); g_gamepad = NULL; }
    int count = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    if (ids && count > 0) g_gamepad = SDL_OpenGamepad(ids[0]);
    SDL_free(ids);
    return g_gamepad;
}

static PadBits Pressed(SDL_Gamepad *pad) {
    PadBits bits = 0;
    for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; ++b) {
        if (SDL_GetGamepadButton(pad, (SDL_GamepadButton)b)) bits |= (PadBits)1 << b;
    }
    if (SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > TRIGGER_DOWN) bits |= BIT_LEFT_TRIGGER;
    if (SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > TRIGGER_DOWN) bits |= BIT_RIGHT_TRIGGER;
    return bits;
}

/* The SDL gamepad name of the first control in bits, or NULL. */
static const char *NameOf(PadBits bits) {
    for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; ++b) {
        if (bits & ((PadBits)1 << b)) return SDL_GetGamepadStringForButton((SDL_GamepadButton)b);
    }
    if (bits & BIT_LEFT_TRIGGER) return "lefttrigger";
    if (bits & BIT_RIGHT_TRIGGER) return "righttrigger";
    return NULL;
}

/* Shows the connected pad's name (or the method) in the Device box. */
static void ShowDevice(void) {
    if (!g_device) return;
    SDL_Gamepad *pad = Pad();
    const char *name = pad ? SDL_GetGamepadName(pad) : NULL;
    wchar_t text[128];
    if (name) swprintf(text, 128, L"%S", name);
    else wcscpy(text, g_method == PES_PAD_DIRECTINPUT ? L"DirectInput: no controller" : L"XInput: no controller");
    SendMessageW(g_device, CB_RESETCONTENT, 0, 0);
    SendMessageW(g_device, CB_ADDSTRING, 0, (LPARAM)text);
    SendMessageW(g_device, CB_SETCURSEL, 0, 0);
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

/* XInput: the fixed standard mapping, the buttons cannot be set by hand. DirectInput: the bindable
 * buttons can be set. Also shows which of the two radio buttons is chosen. */
static void ApplyMethod(void) {
    const int manual = g_method == PES_PAD_DIRECTINPUT;
    for (int s = 0; s < PES_PAD_SLOTS; ++s) EnableWindow(g_binds[s], PesPadBindable(s) && manual);
    SendMessageW(g_direct_radio, BM_SETCHECK, manual ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(g_xinput_radio, BM_SETCHECK, manual ? BST_UNCHECKED : BST_CHECKED, 0);
    ShowDevice();
}

static void StopCapture(HWND page) {
    const int previous = g_capture;
    g_capture = -1;
    KillTimer(page, CAPTURE_TIMER);
    if (previous >= 0) SetText(previous);
}

static void StartCapture(HWND page, int slot) {
    SDL_Gamepad *pad = Pad();
    if (!pad) {
        MessageBoxW(page, L"Connect a controller to set this button.", L"Settings", MB_OK | MB_ICONINFORMATION);
        return;
    }
    StopCapture(page);
    g_baseline = Pressed(pad); /* controls already held do not count */
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
    SDL_Gamepad *pad = Pad();
    if (!pad) return;
    const PadBits now = Pressed(pad);
    const char *name = NameOf(now & ~g_baseline); /* new presses only */
    g_baseline &= now;                             /* a held control that is let go counts again */
    if (name) {
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
        if (id == ID_PAD_DIRECT || id == ID_PAD_XINPUT) {
            const int method = id == ID_PAD_DIRECT ? PES_PAD_DIRECTINPUT : PES_PAD_XINPUT;
            if (method == g_method) return 0;
            StopCapture(hwnd);
            g_method = method;
            if (method == PES_PAD_XINPUT) PesPadDefaults(g_pads); /* back to the standard mapping */
            ApplyMethod();
            RefreshAll();
            return 0;
        }
        if (id >= ID_BIND_BASE && id < ID_BIND_BASE + PES_PAD_SLOTS) {
            const int slot = id - ID_BIND_BASE;
            if (PesPadBindable(slot) && g_method == PES_PAD_DIRECTINPUT) StartCapture(hwnd, slot);
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
        HWND icon = Make(p, L"STATIC", L"", SS_BITMAP, chip_x + 9, y, 40, 26, 0);
        HBITMAP bmp = LoadBitmapW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDB_ICON_BASE + s));
        if (bmp) SendMessageW(icon, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bmp);
        g_binds[s] = Make(p, L"BUTTON", L"", BS_PUSHBUTTON | WS_TABSTOP, button_x, y, 130, 26, ID_BIND_BASE + s);
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
    g_direct_radio = Make(p, L"BUTTON", L"DirectInput", BS_AUTORADIOBUTTON | WS_GROUP | WS_TABSTOP, 446, 36, 120, 22, ID_PAD_DIRECT);
    g_xinput_radio = Make(p, L"BUTTON", L"XInput", BS_AUTORADIOBUTTON, 594, 36, 120, 22, ID_PAD_XINPUT);
    Make(p, L"STATIC", L"Device", SS_LEFT, 0, 72, 90, 20, 0);
    /* The connected pad's name (filled by ApplyMethod through ShowDevice). */
    g_device = Make(p, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 100, 68, 629, 120, ID_DEVICE);

    Column(p, kLeft, 21, 86);
    Column(p, kRight, 548, 614);
    HWND picture = Make(p, L"STATIC", L"", SS_BITMAP, 258, 109, 260, 325, 0);
    HBITMAP bmp = LoadBitmapW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDB_CONTROLLER_PAD));
    if (bmp) SendMessageW(picture, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bmp);
    Make(p, L"BUTTON", L"Revert to Default Settings", BS_PUSHBUTTON | WS_TABSTOP, 232, 448, 313, 30, ID_REVERT);

    for (HWND c = GetWindow(p, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) SendMessageW(c, WM_SETFONT, (WPARAM)font, TRUE);
    int method = PES_PAD_XINPUT;
    PesIniLoadPadMethod(PesConfigPath(), &method); /* the saved choice; XInput if the file has none */
    g_method = method;
    PesPadDefaults(g_pads);
    if (g_method == PES_PAD_DIRECTINPUT) {
        int ignored[PES_PAD_SLOTS]; /* the keyboard part belongs to the Keyboard tab */
        PesIniLoadBindings(PesConfigPath(), ignored, g_pads); /* the saved gamepad bindings, if the file has any */
    }
    ApplyMethod();
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

int SettingsControllerMethod(void) {
    return g_method;
}

void SettingsControllerGet(char pads[PES_PAD_SLOTS][PES_PAD_NAME_MAX]) {
    memcpy(pads, g_pads, sizeof(g_pads));
}

void SettingsControllerSet(const char pads[PES_PAD_SLOTS][PES_PAD_NAME_MAX]) {
    StopCapture(g_page);
    memcpy(g_pads, pads, sizeof(g_pads));
    RefreshAll();
}
