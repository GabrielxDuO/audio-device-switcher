#include "tray.h"
#include <shellapi.h>

static NOTIFYICONDATAW g_nid = {};

void TrayInit(HWND hwnd, HICON hIcon, const wchar_t* tip)
{
    g_nid        = {};
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd   = hwnd;
    g_nid.uID    = TRAY_UID;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    g_nid.uCallbackMessage = WM_TRAY;
    g_nid.hIcon  = hIcon;
    g_nid.uVersion = NOTIFYICON_VERSION_4;
    if (tip) wcsncpy_s(g_nid.szTip, tip, _TRUNCATE);

    Shell_NotifyIconW(NIM_ADD, &g_nid);
    Shell_NotifyIconW(NIM_SETVERSION, &g_nid);
}

void TraySetIcon(HWND hwnd, HICON hIcon)
{
    g_nid.hWnd   = hwnd;
    g_nid.uID    = TRAY_UID;
    g_nid.uFlags = NIF_ICON;
    g_nid.hIcon  = hIcon;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

void TraySetTip(HWND hwnd, const wchar_t* tip)
{
    g_nid.hWnd   = hwnd;
    g_nid.uID    = TRAY_UID;
    g_nid.uFlags = NIF_TIP | NIF_SHOWTIP;
    if (tip) wcsncpy_s(g_nid.szTip, tip, _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

void TrayShowBalloon(HWND hwnd, const wchar_t* title, const wchar_t* text, DWORD infoFlags)
{
    NOTIFYICONDATAW nid = {};
    nid.cbSize    = sizeof(nid);
    nid.hWnd      = hwnd;
    nid.uID       = TRAY_UID;
    nid.uFlags    = NIF_INFO;
    nid.dwInfoFlags = infoFlags;
    nid.uTimeout  = 2000;
    if (title) wcsncpy_s(nid.szInfoTitle, title, _TRUNCATE);
    if (text)  wcsncpy_s(nid.szInfo, text, _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void TrayDestroy(HWND hwnd)
{
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd   = hwnd;
    nid.uID    = TRAY_UID;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}
