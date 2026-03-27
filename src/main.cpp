#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shellapi.h>
#include <objbase.h>
#include <mmdeviceapi.h>
#include <strsafe.h>

#include "../res/resource.h"
#include "tray.h"
#include "audio.h"
#include "startup.h"

// ---------------------------------------------------------------------------
// Menu command IDs
// ---------------------------------------------------------------------------
// Device items: category base + per-device index (max 256 devices per category)
#define ID_DEV_RENDER_CONSOLE       0x1000  // eRender / eConsole
#define ID_DEV_RENDER_COMMS         0x1100  // eRender / eCommunications
#define ID_DEV_CAPTURE_CONSOLE      0x1200  // eCapture / eConsole
#define ID_DEV_CAPTURE_COMMS        0x1300  // eCapture / eCommunications
#define ID_DEV_MASK                 0xFF00

#define ID_REFRESH                  0x2001
#define ID_STARTUP                  0x2002
#define ID_EXIT                     0x2003

static constexpr wchar_t kMutexName[]   = L"AudioDeviceSwitcher_SingleInstance";
static constexpr wchar_t kClassName[]   = L"AudioDeviceSwitcherMsg";
static constexpr wchar_t kAppTip[]      = L"AudioDeviceSwitcher";

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
static HINSTANCE g_hInst      = nullptr;
static HWND      g_hwnd       = nullptr;
static HICON     g_hIconLight = nullptr;
static HICON     g_hIconDark  = nullptr;
static bool      g_isDarkMode = false;

// Undocumented UxTheme exports (ordinals stable since Windows 10 1903).
// Used to opt the process into dark-mode popup menus and flush cached themes
// when the user switches color scheme at runtime. Loaded dynamically so the
// app continues to work on older systems where they don't exist.
using FnSetPreferredAppMode = int  (WINAPI*)(int);  // ordinal 135
using FnFlushMenuThemes     = void (WINAPI*)();     // ordinal 136
static FnFlushMenuThemes g_flushMenuThemes = nullptr;

static void InitDarkModeMenuSupport()
{
    HMODULE hUx = GetModuleHandleW(L"uxtheme.dll");
    if (!hUx) hUx = LoadLibraryW(L"uxtheme.dll");
    if (!hUx) return;

    auto setMode = reinterpret_cast<FnSetPreferredAppMode>(
        GetProcAddress(hUx, MAKEINTRESOURCEA(135)));
    g_flushMenuThemes = reinterpret_cast<FnFlushMenuThemes>(
        GetProcAddress(hUx, MAKEINTRESOURCEA(136)));

    // AllowDark (1): menus follow the system color scheme automatically
    if (setMode) setMode(1);
    if (g_flushMenuThemes) g_flushMenuThemes();
}

// Cached device lists rebuilt each time the menu opens
struct DevEntry {
    std::wstring id;
    std::wstring name;
};
static std::vector<DevEntry> g_renderDevices;
static std::vector<DevEntry> g_captureDevices;

// ---------------------------------------------------------------------------
// Dark mode detection
// ---------------------------------------------------------------------------
static bool IsSystemInDarkMode()
{
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                      0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return false;

    DWORD value = 1;
    DWORD size  = sizeof(value);
    RegQueryValueExW(hKey, L"SystemUsesLightTheme", nullptr, nullptr,
                     reinterpret_cast<BYTE*>(&value), &size);
    RegCloseKey(hKey);
    return (value == 0);
}

// ---------------------------------------------------------------------------
// Menu helpers
// ---------------------------------------------------------------------------
static void AppendDeviceItems(HMENU hMenu, UINT baseId,
                               EDataFlow flow, ERole role,
                               const std::vector<DevEntry>& devices)
{
    if (devices.empty()) {
        AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, L"未找到设备");
        return;
    }

    std::wstring defaultId = GetDefaultDeviceId(flow, role);

    for (UINT i = 0; i < static_cast<UINT>(devices.size()); ++i) {
        UINT flags = MF_STRING;
        if (devices[i].id == defaultId) flags |= MF_CHECKED;
        AppendMenuW(hMenu, flags, baseId + i, devices[i].name.c_str());
    }
}

static void ShowContextMenu(HWND hwnd)
{
    // Refresh device lists
    auto rawRender  = EnumerateDevices(eRender);
    auto rawCapture = EnumerateDevices(eCapture);

    g_renderDevices.clear();
    for (auto& d : rawRender)  g_renderDevices.push_back({ d.id, d.name });
    g_captureDevices.clear();
    for (auto& d : rawCapture) g_captureDevices.push_back({ d.id, d.name });

    HMENU hMenu = CreatePopupMenu();

    // ---- 播放设备 ----
    HMENU hRenderConsole = CreatePopupMenu();
    AppendDeviceItems(hRenderConsole, ID_DEV_RENDER_CONSOLE, eRender, eConsole, g_renderDevices);
    AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hRenderConsole), L"播放设备");

    // ---- 播放通信设备 ----
    HMENU hRenderComms = CreatePopupMenu();
    AppendDeviceItems(hRenderComms, ID_DEV_RENDER_COMMS, eRender, eCommunications, g_renderDevices);
    AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hRenderComms), L"播放通信设备");

    // ---- 录制设备 ----
    HMENU hCaptureConsole = CreatePopupMenu();
    AppendDeviceItems(hCaptureConsole, ID_DEV_CAPTURE_CONSOLE, eCapture, eConsole, g_captureDevices);
    AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hCaptureConsole), L"录制设备");

    // ---- 录制通信设备 ----
    HMENU hCaptureComms = CreatePopupMenu();
    AppendDeviceItems(hCaptureComms, ID_DEV_CAPTURE_COMMS, eCapture, eCommunications, g_captureDevices);
    AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hCaptureComms), L"录制通信设备");

    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, ID_REFRESH, L"刷新设备列表");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

    UINT startupFlags = MF_STRING | (IsStartupEnabled() ? MF_CHECKED : 0);
    AppendMenuW(hMenu, startupFlags, ID_STARTUP, L"开机启动");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, ID_EXIT, L"退出");

    POINT pt = {};
    GetCursorPos(&pt);

    // Required: foreground window so the menu dismisses on click-outside
    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_RIGHTALIGN,
                   pt.x, pt.y, 0, hwnd, nullptr);
    PostMessageW(hwnd, WM_NULL, 0, 0);

    DestroyMenu(hMenu);
}

// ---------------------------------------------------------------------------
// WndProc
// ---------------------------------------------------------------------------
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_TRAY:
        switch (LOWORD(lParam)) {
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
            ShowContextMenu(hwnd);
            break;
        }
        return 0;

    case WM_COMMAND: {
        UINT id = LOWORD(wParam);

        if (id == ID_EXIT) {
            TrayDestroy(hwnd);
            PostQuitMessage(0);
            return 0;
        }

        if (id == ID_REFRESH) {
            TrayShowBalloon(hwnd, L"刷新成功", L"设备列表已更新", NIIF_INFO);
            return 0;
        }

        if (id == ID_STARTUP) {
            bool cur = IsStartupEnabled();
            SetStartup(!cur);
            return 0;
        }

        // Device selection
        auto handleDevice = [&](UINT base, EDataFlow flow, ERole role,
                                 const std::vector<DevEntry>& devices) -> bool {
            if (id < base || id >= base + 0x100) return false;
            UINT idx = id - base;
            if (idx >= devices.size()) return false;
            const std::wstring& devId = devices[idx].id;
            if (SetDefaultDevice(devId, role)) {
                wchar_t msg2[256];
                StringCchPrintfW(msg2, 256, L"已切换到: %s", devices[idx].name.c_str());
                TrayShowBalloon(hwnd, L"切换成功", msg2, NIIF_INFO);
            }
            return true;
        };

        if (!handleDevice(ID_DEV_RENDER_CONSOLE,  eRender,  eConsole,       g_renderDevices) &&
            !handleDevice(ID_DEV_RENDER_COMMS,    eRender,  eCommunications, g_renderDevices) &&
            !handleDevice(ID_DEV_CAPTURE_CONSOLE, eCapture, eConsole,       g_captureDevices))
             handleDevice(ID_DEV_CAPTURE_COMMS,   eCapture, eCommunications, g_captureDevices);
        return 0;
    }

    case WM_SETTINGCHANGE:
        // "ImmersiveColorSet" is broadcast when the system color scheme changes
        if (lParam && lstrcmpiW(reinterpret_cast<LPCWSTR>(lParam), L"ImmersiveColorSet") == 0) {
            bool newDark = IsSystemInDarkMode();
            if (newDark != g_isDarkMode) {
                g_isDarkMode = newDark;
                TraySetIcon(hwnd, g_isDarkMode ? g_hIconDark : g_hIconLight);
                // Flush cached menu theme so the next popup reflects the new color scheme
                if (g_flushMenuThemes) g_flushMenuThemes();
            }
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// WinMain
// ---------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
    // Single-instance guard
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, kMutexName);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex) CloseHandle(hMutex);
        MessageBoxW(nullptr,
                    L"AudioDeviceSwitcher 已经在运行中！\n\n请在系统托盘查找图标。",
                    L"提示", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    g_hInst = hInstance;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    InitDarkModeMenuSupport();

    // Load icons at the DPI-aware tray icon size so they stay crisp on HiDPI displays
    int iconSize = GetSystemMetrics(SM_CXSMICON);
    g_hIconLight = static_cast<HICON>(LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_ICON_LIGHT),
                                                 IMAGE_ICON, iconSize, iconSize, LR_DEFAULTCOLOR));
    g_hIconDark  = static_cast<HICON>(LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_ICON_DARK),
                                                 IMAGE_ICON, iconSize, iconSize, LR_DEFAULTCOLOR));
    if (!g_hIconLight) g_hIconLight = LoadIconW(nullptr, IDI_APPLICATION);
    if (!g_hIconDark)  g_hIconDark  = g_hIconLight;

    g_isDarkMode = IsSystemInDarkMode();

    // Register message-only window class
    WNDCLASSEXW wc  = { sizeof(wc) };
    wc.lpfnWndProc  = WndProc;
    wc.hInstance    = hInstance;
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);

    // HWND_MESSAGE: invisible, no taskbar entry
    g_hwnd = CreateWindowExW(0, kClassName, nullptr, 0,
                              0, 0, 0, 0,
                              HWND_MESSAGE, nullptr, hInstance, nullptr);

    TrayInit(g_hwnd, g_isDarkMode ? g_hIconDark : g_hIconLight, kAppTip);

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CoUninitialize();
    if (hMutex) { ReleaseMutex(hMutex); CloseHandle(hMutex); }
    return static_cast<int>(msg.wParam);
}
