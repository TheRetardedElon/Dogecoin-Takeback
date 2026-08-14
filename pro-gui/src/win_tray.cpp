#if defined(_WIN32)
#include "win_tray.h"

#include <shellapi.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cstring>

#ifndef NIM_SETVERSION
#define NIM_SETVERSION 0x00000004
#endif
#ifndef NOTIFYICON_VERSION_4
#define NOTIFYICON_VERSION_4 4
#endif
#ifndef NIF_SHOWTIP
#define NIF_SHOWTIP 0x00000080
#endif
#ifndef NIN_SELECT
#define NIN_SELECT (WM_USER + 0)
#endif

static const UINT WM_TRAY = WM_APP + 42;
static const UINT ID_TRAY = 1;
static const UINT IDM_SHOW = 1001;
static const UINT IDM_TUI = 1002;
static const UINT IDM_QUIT = 1003;

static HWND g_hwnd = nullptr;
static NOTIFYICONDATAA g_nid {};
static bool g_added = false;
static bool g_hybrid = false;
static volatile LONG g_show = 0;
static volatile LONG g_tui = 0;
static volatile LONG g_quit = 0;
static void ShowMenu()
{
    POINT p;
    GetCursorPos(&p);
    HMENU m = CreatePopupMenu();
    AppendMenuA(m, MF_STRING, IDM_SHOW, "Show Desktop GUI");
    if (g_hybrid)
        AppendMenuA(m, MF_STRING, IDM_TUI, "Open Operator TUI");
    AppendMenuA(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(m, MF_STRING, IDM_QUIT, "Quit and stop node");
    SetForegroundWindow(g_hwnd);
    UINT cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_NONOTIFY, p.x, p.y, 0, g_hwnd, nullptr);
    DestroyMenu(m);
    if (cmd == IDM_SHOW) InterlockedExchange(&g_show, 1);
    else if (cmd == IDM_TUI) InterlockedExchange(&g_tui, 1);
    else if (cmd == IDM_QUIT) InterlockedExchange(&g_quit, 1);
}

bool WinTrayHandleMessage(UINT msg, WPARAM w, LPARAM l)
{
    if (msg == WM_TRAY) {
        UINT ev = LOWORD(l);
        if (ev == WM_LBUTTONDBLCLK || ev == NIN_SELECT || ev == WM_LBUTTONUP)
            InterlockedExchange(&g_show, 1);
        if (ev == WM_RBUTTONUP || ev == WM_CONTEXTMENU)
            ShowMenu();
        return true;
    }
    if (msg == WM_COMMAND) {
        UINT id = LOWORD(w);
        if (id == IDM_SHOW) { InterlockedExchange(&g_show, 1); return true; }
        if (id == IDM_TUI) { InterlockedExchange(&g_tui, 1); return true; }
        if (id == IDM_QUIT) { InterlockedExchange(&g_quit, 1); return true; }
    }
    return false;
}

UINT WinTrayCallbackMsg() { return WM_TRAY; }

bool WinTrayInit(HWND hwnd, bool hybrid, bool testnet)
{
    g_hwnd = hwnd;
    g_hybrid = hybrid;
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = hwnd;
    g_nid.uID = ID_TRAY;
    g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAY;
    g_nid.hIcon = (HICON)SendMessage(hwnd, WM_GETICON, ICON_SMALL, 0);
    if (!g_nid.hIcon)
        g_nid.hIcon = LoadIcon(GetModuleHandleA(nullptr), MAKEINTRESOURCE(1));
    if (!g_nid.hIcon)
        g_nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    const char* tip = testnet ? "Dogecoin Core Pro  —  TESTNET" : "Dogecoin Core Pro";
    strncpy(g_nid.szTip, tip, sizeof(g_nid.szTip) - 1);
    g_nid.szTip[sizeof(g_nid.szTip) - 1] = 0;
    return true;
}

void WinTrayShow()
{
    if (!g_hwnd) return;
    if (!g_added) {
        if (Shell_NotifyIconA(NIM_ADD, &g_nid))
            g_added = true;
    }
}

void WinTrayHide()
{
    if (g_added) {
        Shell_NotifyIconA(NIM_DELETE, &g_nid);
        g_added = false;
    }
}

void WinTrayNotify(const char* title, const char* body)
{
    if (!g_added) WinTrayShow();
    g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_INFO;
    strncpy(g_nid.szInfoTitle, title ? title : "Dogecoin Core Pro", sizeof(g_nid.szInfoTitle) - 1);
    g_nid.szInfoTitle[sizeof(g_nid.szInfoTitle) - 1] = 0;
    strncpy(g_nid.szInfo, body ? body : "", sizeof(g_nid.szInfo) - 1);
    g_nid.szInfo[sizeof(g_nid.szInfo) - 1] = 0;
    g_nid.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconA(NIM_MODIFY, &g_nid);
    g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
}

void WinTrayShutdown()
{
    WinTrayHide();
}

void WinTrayDismissHelper()
{
    // gpenode-tray is a second NotifyIcon whose Exit only kills that helper.
    // This process owns the one tray icon; its Quit stops the node.
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return;
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, L"gpenode-tray.exe") != 0)
                continue;
            HANDLE p = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
            if (p) {
                TerminateProcess(p, 0);
                CloseHandle(p);
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
}

bool WinTrayPollShow() { return InterlockedExchange(&g_show, 0) != 0; }
bool WinTrayPollOpenTui() { return InterlockedExchange(&g_tui, 0) != 0; }
bool WinTrayPollQuit() { return InterlockedExchange(&g_quit, 0) != 0; }

#endif
