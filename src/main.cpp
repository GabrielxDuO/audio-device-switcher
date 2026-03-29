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

#define ID_OPEN_SOUND_SETTINGS      0x2000
#define ID_REFRESH                  0x2001
#define ID_STARTUP                  0x2002
#define ID_EXIT                     0x2003

#define WM_AUDIO_DEVICE_CHANGED     (WM_APP + 2)

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
static UINT      g_wmTaskbarCreated = 0; // registered when Explorer (re)creates the taskbar

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

// Cached device lists, kept up-to-date by IMMNotificationClient
struct DevEntry {
    std::wstring id;
    std::wstring name;
};
static std::vector<DevEntry> g_renderDevices;
static std::vector<DevEntry> g_captureDevices;

// Persistent enumerator used for notification registration
static IMMDeviceEnumerator* g_pEnumerator = nullptr;

// ---------------------------------------------------------------------------
// IMMNotificationClient – posts WM_AUDIO_DEVICE_CHANGED to the main window
// so device list refresh always happens on the UI thread.
// ---------------------------------------------------------------------------
class DeviceNotificationClient : public IMMNotificationClient {
    long m_ref = 1;
public:
    ULONG STDMETHODCALLTYPE AddRef()  override { return InterlockedIncrement(&m_ref); }
    ULONG STDMETHODCALLTYPE Release() override {
        long r = InterlockedDecrement(&m_ref);
        if (r == 0) delete this;
        return r;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IMMNotificationClient)) {
            *ppv = static_cast<IMMNotificationClient*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override {
        PostMessageW(g_hwnd, WM_AUDIO_DEVICE_CHANGED, 0, 0);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override {
        PostMessageW(g_hwnd, WM_AUDIO_DEVICE_CHANGED, 0, 0);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override {
        PostMessageW(g_hwnd, WM_AUDIO_DEVICE_CHANGED, 0, 0);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow, ERole, LPCWSTR) override {
        PostMessageW(g_hwnd, WM_AUDIO_DEVICE_CHANGED, 0, 0);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override {
        PostMessageW(g_hwnd, WM_AUDIO_DEVICE_CHANGED, 0, 0);
        return S_OK;
    }
};

static DeviceNotificationClient* g_pNotifyClient = nullptr;

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
// Theme-change helper (called from registry watcher and WM_SETTINGCHANGE)
// ---------------------------------------------------------------------------
static void CheckAndUpdateDarkMode()
{
    bool newDark = IsSystemInDarkMode();
    if (newDark != g_isDarkMode) {
        g_isDarkMode = newDark;
        TraySetIcon(g_hwnd, g_isDarkMode ? g_hIconDark : g_hIconLight);
        if (g_flushMenuThemes) g_flushMenuThemes();
    }
}

// ---------------------------------------------------------------------------
// Tray tooltip – shows current default devices for all four categories
// ---------------------------------------------------------------------------
static void UpdateTrayTip()
{
    std::wstring renderCon  = GetDefaultDeviceName(eRender,  eConsole);
    std::wstring renderComm = GetDefaultDeviceName(eRender,  eCommunications);
    std::wstring capCon     = GetDefaultDeviceName(eCapture, eConsole);
    std::wstring capComm    = GetDefaultDeviceName(eCapture, eCommunications);

    if (renderCon.empty())  renderCon  = L"无";
    if (renderComm.empty()) renderComm = L"无";
    if (capCon.empty())     capCon     = L"无";
    if (capComm.empty())    capComm    = L"无";

    // szTip is 128 wchars (including null terminator).
    // Truncate device names if the full string would exceed the limit.
    wchar_t tip[128];
    StringCchPrintfW(tip, _countof(tip),
        L"播放设备：%s\n播放通信设备：%s\n录制设备：%s\n录制通信设备：%s",
        renderCon.c_str(), renderComm.c_str(),
        capCon.c_str(), capComm.c_str());

    TraySetTip(g_hwnd, tip);
}

// ---------------------------------------------------------------------------
// Device list cache
// ---------------------------------------------------------------------------
static void RefreshDeviceLists()
{
    auto rawRender  = EnumerateDevices(eRender);
    auto rawCapture = EnumerateDevices(eCapture);

    g_renderDevices.clear();
    for (auto& d : rawRender)  g_renderDevices.push_back({ std::move(d.id), std::move(d.name) });
    g_captureDevices.clear();
    for (auto& d : rawCapture) g_captureDevices.push_back({ std::move(d.id), std::move(d.name) });
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
    AppendMenuW(hMenu, MF_STRING, ID_OPEN_SOUND_SETTINGS, L"打开声音设置...");
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
    // TaskbarCreated is a registered message (dynamic ID), can't be a case label.
    // Explorer broadcasts it to all top-level windows after it restarts, at which
    // point every tray icon has been destroyed and must be re-added.
    if (msg == g_wmTaskbarCreated) {
        TrayInit(hwnd, g_isDarkMode ? g_hIconDark : g_hIconLight, kAppTip);
        UpdateTrayTip();
        return 0;
    }

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

        if (id == ID_OPEN_SOUND_SETTINGS) {
            ShellExecuteW(nullptr, L"open", L"control.exe", L"mmsys.cpl",
                          nullptr, SW_SHOWNORMAL);
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
                UpdateTrayTip();
            }
            return true;
        };

        if (!handleDevice(ID_DEV_RENDER_CONSOLE,  eRender,  eConsole,       g_renderDevices) &&
            !handleDevice(ID_DEV_RENDER_COMMS,    eRender,  eCommunications, g_renderDevices) &&
            !handleDevice(ID_DEV_CAPTURE_CONSOLE, eCapture, eConsole,       g_captureDevices))
             handleDevice(ID_DEV_CAPTURE_COMMS,   eCapture, eCommunications, g_captureDevices);
        return 0;
    }

    case WM_AUDIO_DEVICE_CHANGED:
        RefreshDeviceLists();
        UpdateTrayTip();
        return 0;

    case WM_SETTINGCHANGE:
        if (lParam && lstrcmpiW(reinterpret_cast<LPCWSTR>(lParam), L"ImmersiveColorSet") == 0)
            CheckAndUpdateDarkMode();
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

    // Register TaskbarCreated before creating the window so no broadcast is missed.
    // Must be done before the window exists; the ID is stable for the process lifetime.
    g_wmTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    // Regular invisible top-level window (NOT HWND_MESSAGE): HWND_MESSAGE windows
    // are excluded from HWND_BROADCAST, so they never receive TaskbarCreated.
    // No WS_VISIBLE / WS_CAPTION / WS_EX_APPWINDOW = invisible, no taskbar entry,
    // no Alt+Tab entry.
    g_hwnd = CreateWindowExW(0, kClassName, nullptr, 0,
                              0, 0, 0, 0,
                              nullptr, nullptr, hInstance, nullptr);

    // Create persistent enumerator and register for device-change notifications.
    // This keeps g_renderDevices / g_captureDevices up-to-date in the background
    // so the context menu opens instantly without re-enumerating.
    CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                     __uuidof(IMMDeviceEnumerator),
                     reinterpret_cast<void**>(&g_pEnumerator));
    if (g_pEnumerator) {
        g_pNotifyClient = new DeviceNotificationClient();
        g_pEnumerator->RegisterEndpointNotificationCallback(g_pNotifyClient);
    }

    RefreshDeviceLists();

    TrayInit(g_hwnd, g_isDarkMode ? g_hIconDark : g_hIconLight, kAppTip);
    UpdateTrayTip();

    // Watch the Personalize registry key so the tray icon updates reliably
    // when the user switches light / dark mode.  WM_SETTINGCHANGE is kept as
    // a secondary trigger, but it is not always delivered to invisible windows.
    HKEY   hThemeKey   = nullptr;
    HANDLE hThemeEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    bool   watchingReg = false;

    if (hThemeEvent &&
        RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                      0, KEY_NOTIFY, &hThemeKey) == ERROR_SUCCESS &&
        RegNotifyChangeKeyValue(hThemeKey, FALSE, REG_NOTIFY_CHANGE_LAST_SET,
                                hThemeEvent, TRUE) == ERROR_SUCCESS) {
        watchingReg = true;
    }

    int exitCode = 0;
    for (bool quit = false; !quit; ) {
        DWORD nCount = watchingReg ? 1 : 0;
        DWORD result = MsgWaitForMultipleObjects(
            nCount, watchingReg ? &hThemeEvent : nullptr,
            FALSE, INFINITE, QS_ALLINPUT);

        if (watchingReg && result == WAIT_OBJECT_0) {
            CheckAndUpdateDarkMode();
            RegNotifyChangeKeyValue(hThemeKey, FALSE, REG_NOTIFY_CHANGE_LAST_SET,
                                    hThemeEvent, TRUE);
        }

        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                exitCode = static_cast<int>(msg.wParam);
                quit = true;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (g_pEnumerator && g_pNotifyClient)
        g_pEnumerator->UnregisterEndpointNotificationCallback(g_pNotifyClient);
    if (g_pNotifyClient) { g_pNotifyClient->Release(); g_pNotifyClient = nullptr; }
    if (g_pEnumerator)   { g_pEnumerator->Release();   g_pEnumerator   = nullptr; }

    if (hThemeKey)   RegCloseKey(hThemeKey);
    if (hThemeEvent) CloseHandle(hThemeEvent);
    CoUninitialize();
    if (hMutex) { ReleaseMutex(hMutex); CloseHandle(hMutex); }
    return exitCode;
}
