#pragma once
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

bool WinTrayInit(HWND hwnd, bool hybrid, bool testnet = false);
void WinTrayShow();
void WinTrayHide();
void WinTrayNotify(const char* title, const char* body);
void WinTrayShutdown();
/** Drop leftover gpenode-tray.exe so this process is the only tray icon. */
void WinTrayDismissHelper();
bool WinTrayPollShow();
bool WinTrayPollOpenTui();
bool WinTrayPollQuit();
bool WinTrayHandleMessage(UINT msg, WPARAM w, LPARAM l);
UINT WinTrayCallbackMsg();
#endif
