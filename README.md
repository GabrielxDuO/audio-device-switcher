# AudioDeviceSwitcher

Windows 音频设备快速切换工具

[![Version](https://img.shields.io/badge/version-2.2.0-blue)](https://github.com/GabrielxDuO/audio-device-switcher/releases)
[![Build](https://github.com/GabrielxDuO/audio-device-switcher/workflows/Build/badge.svg)](https://github.com/GabrielxDuO/audio-device-switcher/actions)
[![Platform](https://img.shields.io/badge/platform-Windows%2010%2F11-lightgrey)](https://github.com/GabrielxDuO/audio-device-switcher)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

## 特性

- 快速切换 播放设备 / 播放通信设备 / 录制设备 / 录制通信设备
- 一键打开系统声音设置面板
- 开机自启动（可选）
- 暗色模式图标实时响应系统主题切换
- **零依赖**：无需任何运行时，直接双击运行
- 极致轻量：文件大小约 40 KB，内存占用约 2~5 MB

## 快速开始

### 下载

前往 [Releases](https://github.com/GabrielxDuO/audio-device-switcher/releases) 页面下载最新 `AudioDeviceSwitcher.exe`。

### 使用

双击运行，程序出现在系统托盘。

- **左键 / 右键点击托盘图标**：打开菜单
- **选择设备**：立即切换
- **打开声音设置**：快速跳转系统声音控制面板
- **开机启动**：勾选菜单中的选项
- **退出**：点击菜单中的退出

## 系统要求

- Windows 10 / 11（64 位）
- 无需安装任何运行时

## 从源码构建

### 前置条件

- Visual Studio 2022（含 C++ 桌面开发工作负荷）
- CMake 3.20+

### 构建

```bat
.\build.bat
```

或手动：

```bat
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

产物位于 `build\Release\AudioDeviceSwitcher.exe`。

## 技术栈

- C++17
- 纯 Win32 API（无第三方库）
- `IMMDeviceEnumerator`（公开 API）枚举音频设备
- `IPolicyConfig`（未公开但稳定的 COM 接口）设置默认设备
- `Shell_NotifyIcon` 托盘图标
- MSVC `/MT /O1 /SUBSYSTEM:WINDOWS` 编译，静态链接 CRT

## 项目结构

```
audio-device-switcher/
├── src/
│   ├── main.cpp          # WinMain + 消息循环 + WndProc
│   ├── tray.h/cpp        # Shell_NotifyIcon 封装
│   ├── audio.h/cpp       # 设备枚举 + IPolicyConfig 切换
│   ├── startup.h/cpp     # 注册表开机启动
│   └── PolicyConfig.h    # IPolicyConfig COM 接口声明
├── res/
│   ├── resource.h        # 资源 ID 常量
│   ├── resource.rc       # 嵌入图标 + VERSIONINFO
│   ├── icon.ico          # 亮色模式图标
│   └── icon-dark.ico     # 暗色模式图标
├── .github/workflows/
│   ├── build.yml         # CI
│   └── release.yml       # CD
├── CMakeLists.txt
└── build.bat
```

## 许可证

MIT License
