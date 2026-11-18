/* SPDX-License-Identifier: GPL-2.0-or-later */
/* The Audio tab of the settings window (M6.5a): Sound Properties (opens the Windows sound panel), the
 * audio buffer (1..8, saved only), and the audio test: six buttons, each playing a short test tone on
 * its speaker through the default output. The tones come from settings_tone (tested everywhere). */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include "settings_audio.h"
#include "settings_ini.h"
#include "settings_model.h"
#include "settings_tone.h"

#define PAGE_X 24
#define PAGE_Y 150
#define PAGE_W 752
#define PAGE_H 506
#define ID_SOUND_PROPS 1000
#define ID_BUFFER 1001
#define ID_TEST_BASE 1010 /* test button of speaker s is ID_TEST_BASE + s */

/* One tone file at a time: PlaySound plays it from this buffer while it runs, so it stays static. */
static unsigned char g_tone[44 + 48000 * 2 * 2];

static const wchar_t *kAudioClass = L"PesSettingsAudio";
static const wchar_t *kSpeakerLabels[PES_SPK_COUNT] = {L"L", L"R", L"C", L"Lfe", L"Ls", L"Rs"};
static HWND g_page, g_buffer;

static HWND Make(HWND parent, const wchar_t *cls, const wchar_t *text, DWORD style, int x, int y, int w, int h, int id) {
    return CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, parent,
                           (HMENU)(INT_PTR)id, GetModuleHandleW(NULL), NULL);
}

/* Plays the test tone of one speaker through the default output. Without an output device nothing
 * plays, and the button does nothing. */
static void PlayTone(int speaker) {
    if (PesToneWav(speaker, g_tone, sizeof(g_tone)) == 0) return;
    PlaySoundW((LPCWSTR)g_tone, NULL, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
}

static LRESULT CALLBACK AudioPageProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_COMMAND: {
        const int id = LOWORD(wp);
        if (HIWORD(wp) != BN_CLICKED) break;
        if (id == ID_SOUND_PROPS) {
            ShellExecuteW(hwnd, L"open", L"control.exe", L"mmsys.cpl", NULL, SW_SHOWNORMAL);
            return 0;
        }
        if (id >= ID_TEST_BASE && id < ID_TEST_BASE + PES_SPK_COUNT) {
            PlayTone(id - ID_TEST_BASE);
            return 0;
        }
        break;
    }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void RegisterAudioClass(void) {
    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    if (GetClassInfoExW(GetModuleHandleW(NULL), kAudioClass, &wc)) return;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = AudioPageProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = kAudioClass;
    RegisterClassExW(&wc);
}

/* A speaker's test button in the grid under the layout pictures: column 0 left, 1 centre, 2 right. */
static void TestButton(HWND p, int speaker, int column, int row) {
    /* Columns at x 226, 326 and 426, each 100 wide: the centre column is centred on the page (x 376). */
    const int x = 226 + column * 100;
    const int y = 330 + row * 42;
    Make(p, L"BUTTON", kSpeakerLabels[speaker], BS_PUSHBUTTON | WS_TABSTOP, x, y, 100, 28, ID_TEST_BASE + speaker);
}

void SettingsAudioCreate(HWND parent, HFONT font) {
    RegisterAudioClass();
    g_page = CreateWindowExW(0, kAudioClass, L"", WS_CHILD | WS_CLIPCHILDREN, PAGE_X, PAGE_Y, PAGE_W, PAGE_H,
                             parent, NULL, GetModuleHandleW(NULL), NULL);
    HWND p = g_page;

    /* The three groups first, so the controls inside them paint over them. */
    Make(p, L"BUTTON", L"Sound Properties", BS_GROUPBOX, 0, 0, 240, 96, 0);
    Make(p, L"BUTTON", L"Audio Buffer", BS_GROUPBOX, 256, 0, 496, 96, 0);
    Make(p, L"BUTTON", L"Audio Test", BS_GROUPBOX, 0, 116, 752, 380, 0);

    Make(p, L"BUTTON", L"Sound Properties", BS_PUSHBUTTON | WS_TABSTOP, 16, 36, 208, 30, ID_SOUND_PROPS);

    g_buffer = Make(p, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, 272, 36, 96, 200, ID_BUFFER);
    for (int i = 1; i <= 8; ++i) {
        wchar_t item[4];
        swprintf(item, 4, L"%d", i);
        SendMessageW(g_buffer, CB_ADDSTRING, 0, (LPARAM)item);
    }
    Make(p, L"STATIC", L"Please adjust this setting if you experience any sound skipping when playing the game.",
         SS_LEFT, 384, 30, 352, 56, 0);

    /* The layout pictures: the speakers as in the reference (stereo/2.1 on the left, surround on the right).
     * They show the layout only; the test buttons below play the tones. */
    Make(p, L"STATIC", L"", SS_GRAYFRAME, 40, 140, 672, 180, 0);
    Make(p, L"STATIC", L"Stereo/2.1", SS_CENTER, 60, 296, 300, 20, 0);
    Make(p, L"STATIC", L"Surround", SS_CENTER, 392, 296, 300, 20, 0);
    Make(p, L"STATIC", kSpeakerLabels[PES_SPK_L], SS_CENTER | WS_BORDER, 100, 240, 80, 26, 0);
    Make(p, L"STATIC", kSpeakerLabels[PES_SPK_LFE], SS_CENTER | WS_BORDER, 180, 190, 80, 26, 0);
    Make(p, L"STATIC", kSpeakerLabels[PES_SPK_R], SS_CENTER | WS_BORDER, 260, 240, 80, 26, 0);
    Make(p, L"STATIC", kSpeakerLabels[PES_SPK_C], SS_CENTER | WS_BORDER, 530, 150, 80, 26, 0);
    Make(p, L"STATIC", kSpeakerLabels[PES_SPK_L], SS_CENTER | WS_BORDER, 450, 200, 80, 26, 0);
    Make(p, L"STATIC", kSpeakerLabels[PES_SPK_LFE], SS_CENTER | WS_BORDER, 530, 200, 80, 26, 0);
    Make(p, L"STATIC", kSpeakerLabels[PES_SPK_R], SS_CENTER | WS_BORDER, 610, 200, 80, 26, 0);
    Make(p, L"STATIC", kSpeakerLabels[PES_SPK_LS], SS_CENTER | WS_BORDER, 450, 240, 80, 26, 0);
    Make(p, L"STATIC", kSpeakerLabels[PES_SPK_RS], SS_CENTER | WS_BORDER, 610, 240, 80, 26, 0);

    /* The six test buttons: C on top, L Lfe R in the middle, Ls and Rs at the bottom. */
    TestButton(p, PES_SPK_C, 1, 0);
    TestButton(p, PES_SPK_L, 0, 1);
    TestButton(p, PES_SPK_LFE, 1, 1);
    TestButton(p, PES_SPK_R, 2, 1);
    TestButton(p, PES_SPK_LS, 0, 2);
    TestButton(p, PES_SPK_RS, 2, 2);

    for (HWND c = GetWindow(p, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) SendMessageW(c, WM_SETFONT, (WPARAM)font, TRUE);

    /* The saved buffer, 1 if the file has none; clamped to the allowed 1..8. */
    PesDisplaySettings s = PesDisplayDefaults();
    int buffer = s.audio_buffer;
    PesIniLoadInt(PesConfigPath(), "audio_buffer", &buffer);
    s.audio_buffer = buffer;
    s = PesDisplayClamp(s);
    SendMessageW(g_buffer, CB_SETCURSEL, (WPARAM)(s.audio_buffer - 1), 0);

    ShowWindow(g_page, SW_HIDE);
}

void SettingsAudioShow(int visible) {
    if (g_page) ShowWindow(g_page, visible ? SW_SHOW : SW_HIDE);
}

int SettingsAudioBuffer(void) {
    const int sel = (int)SendMessageW(g_buffer, CB_GETCURSEL, 0, 0);
    return sel >= 0 ? sel + 1 : 1;
}
