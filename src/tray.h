#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

// WM_APP+1 is posted to hwnd for all tray icon interactions.
#define WM_TRAY (WM_APP + 1)
#define TRAY_UID 1

void TrayInit(HWND hwnd, HICON hIcon, const wchar_t* tip);
void TraySetIcon(HWND hwnd, HICON hIcon);
void TraySetTip(HWND hwnd, const wchar_t* tip);
void TrayShowBalloon(HWND hwnd, const wchar_t* title, const wchar_t* text, DWORD infoFlags = NIIF_INFO);
void TrayDestroy(HWND hwnd);
