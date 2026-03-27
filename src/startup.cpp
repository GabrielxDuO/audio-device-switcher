#include "startup.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static constexpr wchar_t kRunKey[]  = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";
static constexpr wchar_t kAppName[] = L"AudioDeviceSwitcher";

bool IsStartupEnabled()
{
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return false;

    DWORD type = 0;
    DWORD size = 0;
    bool exists = (RegQueryValueExW(hKey, kAppName, nullptr, &type, nullptr, &size) == ERROR_SUCCESS);
    RegCloseKey(hKey);
    return exists;
}

bool SetStartup(bool enable)
{
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_WRITE, &hKey) != ERROR_SUCCESS)
        return false;

    bool ok = false;
    if (enable) {
        wchar_t exePath[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        DWORD byteLen = static_cast<DWORD>((wcslen(exePath) + 1) * sizeof(wchar_t));
        ok = (RegSetValueExW(hKey, kAppName, 0, REG_SZ,
                             reinterpret_cast<const BYTE*>(exePath), byteLen) == ERROR_SUCCESS);
    } else {
        LSTATUS st = RegDeleteValueW(hKey, kAppName);
        ok = (st == ERROR_SUCCESS || st == ERROR_FILE_NOT_FOUND);
    }

    RegCloseKey(hKey);
    return ok;
}
