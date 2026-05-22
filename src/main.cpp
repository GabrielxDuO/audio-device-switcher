#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shellapi.h>
#include <objbase.h>
#include <mmdeviceapi.h>
#include <strsafe.h>

#include <thread>

#include "../res/resource.h"
#include "tray.h"
#include "audio.h"
#include "startup.h"
#include "i18n.h"

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
#define WM_AUDIO_REFRESH_COMPLETE   (WM_APP + 3)

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
static std::wstring g_renderConsoleDefaultId;
static std::wstring g_renderCommsDefaultId;
static std::wstring g_captureConsoleDefaultId;
static std::wstring g_captureCommsDefaultId;
static std::wstring g_renderConsoleDefaultName;
static std::wstring g_renderCommsDefaultName;
static std::wstring g_captureConsoleDefaultName;
static std::wstring g_captureCommsDefaultName;
static bool g_refreshInProgress = false;
static bool g_refreshAgain = false;
static bool g_showRefreshBalloonAfterRefresh = false;

// Persistent enumerator used for notification registration
static IMMDeviceEnumerator* g_pEnumerator = nullptr;

// ---------------------------------------------------------------------------
// IMMNotificationClient – posts WM_AUDIO_DEVICE_CHANGED to the main window,
// which schedules slow CoreAudio refresh work outside the UI thread.
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
    std::wstring renderCon  = g_renderConsoleDefaultName;
    std::wstring renderComm = g_renderCommsDefaultName;
    std::wstring capCon     = g_captureConsoleDefaultName;
    std::wstring capComm    = g_captureCommsDefaultName;

    const auto& s = GetStrings();
    if (renderCon.empty())  renderCon  = s.none;
    if (renderComm.empty()) renderComm = s.none;
    if (capCon.empty())     capCon     = s.none;
    if (capComm.empty())    capComm    = s.none;

    wchar_t tip[128];
    StringCchPrintfW(tip, _countof(tip), s.tipFormat,
        renderCon.c_str(), renderComm.c_str(),
        capCon.c_str(), capComm.c_str());

    TraySetTip(g_hwnd, tip);
}

// ---------------------------------------------------------------------------
// Device list cache
// ---------------------------------------------------------------------------
struct AudioSnapshot {
    std::vector<DevEntry> renderDevices;
    std::vector<DevEntry> captureDevices;
    std::wstring renderConsoleDefaultId;
    std::wstring renderCommsDefaultId;
    std::wstring captureConsoleDefaultId;
    std::wstring captureCommsDefaultId;
    std::wstring renderConsoleDefaultName;
    std::wstring renderCommsDefaultName;
    std::wstring captureConsoleDefaultName;
    std::wstring captureCommsDefaultName;
};

static AudioSnapshot CollectAudioSnapshot()
{
    AudioSnapshot snapshot;

    auto rawRender  = EnumerateDevices(eRender);
    auto rawCapture = EnumerateDevices(eCapture);

    for (auto& d : rawRender)  snapshot.renderDevices.push_back({ std::move(d.id), std::move(d.name) });
    for (auto& d : rawCapture) snapshot.captureDevices.push_back({ std::move(d.id), std::move(d.name) });

    snapshot.renderConsoleDefaultId  = GetDefaultDeviceId(eRender,  eConsole);
    snapshot.renderCommsDefaultId    = GetDefaultDeviceId(eRender,  eCommunications);
    snapshot.captureConsoleDefaultId = GetDefaultDeviceId(eCapture, eConsole);
    snapshot.captureCommsDefaultId   = GetDefaultDeviceId(eCapture, eCommunications);
    snapshot.renderConsoleDefaultName  = GetDefaultDeviceName(eRender,  eConsole);
    snapshot.renderCommsDefaultName    = GetDefaultDeviceName(eRender,  eCommunications);
    snapshot.captureConsoleDefaultName = GetDefaultDeviceName(eCapture, eConsole);
    snapshot.captureCommsDefaultName   = GetDefaultDeviceName(eCapture, eCommunications);

    return snapshot;
}

static void ApplyAudioSnapshot(AudioSnapshot& snapshot)
{
    g_renderDevices = std::move(snapshot.renderDevices);
    g_captureDevices = std::move(snapshot.captureDevices);
    g_renderConsoleDefaultId = std::move(snapshot.renderConsoleDefaultId);
    g_renderCommsDefaultId = std::move(snapshot.renderCommsDefaultId);
    g_captureConsoleDefaultId = std::move(snapshot.captureConsoleDefaultId);
    g_captureCommsDefaultId = std::move(snapshot.captureCommsDefaultId);
    g_renderConsoleDefaultName = std::move(snapshot.renderConsoleDefaultName);
    g_renderCommsDefaultName = std::move(snapshot.renderCommsDefaultName);
    g_captureConsoleDefaultName = std::move(snapshot.captureConsoleDefaultName);
    g_captureCommsDefaultName = std::move(snapshot.captureCommsDefaultName);
}

static void RefreshDeviceLists()
{
    AudioSnapshot snapshot = CollectAudioSnapshot();
    ApplyAudioSnapshot(snapshot);
}

static void RequestDeviceRefresh(bool showBalloon)
{
    if (showBalloon) g_showRefreshBalloonAfterRefresh = true;

    if (g_refreshInProgress) {
        g_refreshAgain = true;
        return;
    }

    g_refreshInProgress = true;
    std::thread([] {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);
        AudioSnapshot* snapshot = new AudioSnapshot(CollectAudioSnapshot());
        if (SUCCEEDED(hr)) CoUninitialize();
        PostMessageW(g_hwnd, WM_AUDIO_REFRESH_COMPLETE, 0,
                     reinterpret_cast<LPARAM>(snapshot));
    }).detach();
}

// ---------------------------------------------------------------------------
// Menu helpers
// ---------------------------------------------------------------------------
static void AppendDeviceItems(HMENU hMenu, UINT baseId,
                               const std::wstring& defaultId,
                               const std::vector<DevEntry>& devices)
{
    if (devices.empty()) {
        AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, GetStrings().noDeviceFound);
        return;
    }

    for (UINT i = 0; i < static_cast<UINT>(devices.size()); ++i) {
        UINT flags = MF_STRING;
        if (devices[i].id == defaultId) flags |= MF_CHECKED;
        AppendMenuW(hMenu, flags, baseId + i, devices[i].name.c_str());
    }
}

static void ShowContextMenu(HWND hwnd)
{
    HMENU hMenu = CreatePopupMenu();

    const auto& s = GetStrings();

    HMENU hRenderConsole = CreatePopupMenu();
    AppendDeviceItems(hRenderConsole, ID_DEV_RENDER_CONSOLE, g_renderConsoleDefaultId, g_renderDevices);
    AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hRenderConsole), s.playbackDevices);

    HMENU hRenderComms = CreatePopupMenu();
    AppendDeviceItems(hRenderComms, ID_DEV_RENDER_COMMS, g_renderCommsDefaultId, g_renderDevices);
    AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hRenderComms), s.playbackCommDevices);

    HMENU hCaptureConsole = CreatePopupMenu();
    AppendDeviceItems(hCaptureConsole, ID_DEV_CAPTURE_CONSOLE, g_captureConsoleDefaultId, g_captureDevices);
    AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hCaptureConsole), s.recordingDevices);

    HMENU hCaptureComms = CreatePopupMenu();
    AppendDeviceItems(hCaptureComms, ID_DEV_CAPTURE_COMMS, g_captureCommsDefaultId, g_captureDevices);
    AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hCaptureComms), s.recordingCommDevices);

    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, ID_REFRESH, s.refreshDeviceList);
    AppendMenuW(hMenu, MF_STRING, ID_OPEN_SOUND_SETTINGS, s.openSoundSettings);
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

    UINT startupFlags = MF_STRING | (IsStartupEnabled() ? MF_CHECKED : 0);
    AppendMenuW(hMenu, startupFlags, ID_STARTUP, s.startAtLogin);
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, ID_EXIT, s.exit);

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
            RequestDeviceRefresh(true);
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
                if (flow == eRender && role == eConsole) {
                    g_renderConsoleDefaultId = devId;
                    g_renderConsoleDefaultName = devices[idx].name;
                } else if (flow == eRender && role == eCommunications) {
                    g_renderCommsDefaultId = devId;
                    g_renderCommsDefaultName = devices[idx].name;
                } else if (flow == eCapture && role == eConsole) {
                    g_captureConsoleDefaultId = devId;
                    g_captureConsoleDefaultName = devices[idx].name;
                } else if (flow == eCapture && role == eCommunications) {
                    g_captureCommsDefaultId = devId;
                    g_captureCommsDefaultName = devices[idx].name;
                }
                wchar_t msg2[256];
                StringCchPrintfW(msg2, 256, GetStrings().switchedToFmt, devices[idx].name.c_str());
                TrayShowBalloon(hwnd, GetStrings().switchSuccess, msg2, NIIF_INFO);
                UpdateTrayTip();
                RequestDeviceRefresh(false);
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
        RequestDeviceRefresh(false);
        return 0;

    case WM_AUDIO_REFRESH_COMPLETE: {
        AudioSnapshot* snapshot = reinterpret_cast<AudioSnapshot*>(lParam);
        if (snapshot) {
            ApplyAudioSnapshot(*snapshot);
            delete snapshot;
        }

        UpdateTrayTip();
        if (g_showRefreshBalloonAfterRefresh) {
            TrayShowBalloon(hwnd, GetStrings().refreshSuccess, GetStrings().deviceListUpdated, NIIF_INFO);
            g_showRefreshBalloonAfterRefresh = false;
        }

        g_refreshInProgress = false;
        if (g_refreshAgain) {
            g_refreshAgain = false;
            RequestDeviceRefresh(false);
        }
        return 0;
    }

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
                    GetStrings().alreadyRunning,
                    GetStrings().notice, MB_OK | MB_ICONINFORMATION);
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
