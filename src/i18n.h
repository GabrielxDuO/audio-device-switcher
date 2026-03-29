#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

enum class Language { Chinese, English, Japanese };

struct I18nStrings {
    const wchar_t* none;
    const wchar_t* tipFormat;
    const wchar_t* noDeviceFound;
    const wchar_t* playbackDevices;
    const wchar_t* playbackCommDevices;
    const wchar_t* recordingDevices;
    const wchar_t* recordingCommDevices;
    const wchar_t* refreshDeviceList;
    const wchar_t* openSoundSettings;
    const wchar_t* startAtLogin;
    const wchar_t* exit;
    const wchar_t* refreshSuccess;
    const wchar_t* deviceListUpdated;
    const wchar_t* switchedToFmt;
    const wchar_t* switchSuccess;
    const wchar_t* alreadyRunning;
    const wchar_t* notice;
};

inline Language DetectSystemLanguage()
{
    LANGID langId = GetUserDefaultUILanguage();
    switch (PRIMARYLANGID(langId)) {
    case LANG_CHINESE:  return Language::Chinese;
    case LANG_JAPANESE: return Language::Japanese;
    default:            return Language::English;
    }
}

inline const I18nStrings& GetStrings()
{
    static const I18nStrings zh = {
        /* none                */ L"无",
        /* tipFormat           */ L"播放设备：%s\n播放通信设备：%s\n录制设备：%s\n录制通信设备：%s",
        /* noDeviceFound       */ L"未找到设备",
        /* playbackDevices     */ L"播放设备",
        /* playbackCommDevices */ L"播放通信设备",
        /* recordingDevices    */ L"录制设备",
        /* recordingCommDevices*/ L"录制通信设备",
        /* refreshDeviceList   */ L"刷新设备列表",
        /* openSoundSettings   */ L"打开声音设置...",
        /* startAtLogin        */ L"开机启动",
        /* exit                */ L"退出",
        /* refreshSuccess      */ L"刷新成功",
        /* deviceListUpdated   */ L"设备列表已更新",
        /* switchedToFmt       */ L"已切换到: %s",
        /* switchSuccess       */ L"切换成功",
        /* alreadyRunning      */ L"AudioDeviceSwitcher 已经在运行中！\n\n请在系统托盘查找图标。",
        /* notice              */ L"提示",
    };

    static const I18nStrings en = {
        /* none                */ L"None",
        /* tipFormat           */ L"Playback: %s\nPlayback Comm: %s\nRecording: %s\nRecording Comm: %s",
        /* noDeviceFound       */ L"No devices found",
        /* playbackDevices     */ L"Playback Devices",
        /* playbackCommDevices */ L"Playback Comm Devices",
        /* recordingDevices    */ L"Recording Devices",
        /* recordingCommDevices*/ L"Recording Comm Devices",
        /* refreshDeviceList   */ L"Refresh Device List",
        /* openSoundSettings   */ L"Open Sound Settings...",
        /* startAtLogin        */ L"Start at Login",
        /* exit                */ L"Exit",
        /* refreshSuccess      */ L"Refresh Successful",
        /* deviceListUpdated   */ L"Device list updated",
        /* switchedToFmt       */ L"Switched to: %s",
        /* switchSuccess       */ L"Switch Successful",
        /* alreadyRunning      */ L"AudioDeviceSwitcher is already running!\n\nPlease look for its icon in the system tray.",
        /* notice              */ L"Notice",
    };

    static const I18nStrings ja = {
        /* none                */ L"なし",
        /* tipFormat           */ L"再生デバイス：%s\n再生通信デバイス：%s\n録音デバイス：%s\n録音通信デバイス：%s",
        /* noDeviceFound       */ L"デバイスが見つかりません",
        /* playbackDevices     */ L"再生デバイス",
        /* playbackCommDevices */ L"再生通信デバイス",
        /* recordingDevices    */ L"録音デバイス",
        /* recordingCommDevices*/ L"録音通信デバイス",
        /* refreshDeviceList   */ L"デバイスリストを更新",
        /* openSoundSettings   */ L"サウンド設定を開く...",
        /* startAtLogin        */ L"スタートアップに登録",
        /* exit                */ L"終了",
        /* refreshSuccess      */ L"更新成功",
        /* deviceListUpdated   */ L"デバイスリストが更新されました",
        /* switchedToFmt       */ L"切り替え先: %s",
        /* switchSuccess       */ L"切り替え成功",
        /* alreadyRunning      */ L"AudioDeviceSwitcher は既に実行中です！\n\nシステムトレイのアイコンを確認してください。",
        /* notice              */ L"通知",
    };

    static const Language lang = DetectSystemLanguage();
    switch (lang) {
    case Language::Chinese:  return zh;
    case Language::Japanese: return ja;
    default:                 return en;
    }
}
