# AudioDeviceSwitcher

Windows 音频设备快速切换工具

[![Version](https://img.shields.io/badge/version-1.2.0-blue)](https://github.com/GabrielxDuO/audio-device-switcher/releases)
[![Build](https://github.com/GabrielxDuO/audio-device-switcher/workflows/Build/badge.svg)](https://github.com/GabrielxDuO/audio-device-switcher/actions)
[![Platform](https://img.shields.io/badge/platform-Windows%2010%2F11-lightgrey)](https://github.com/GabrielxDuO/audio-device-switcher)
[![Framework](https://img.shields.io/badge/.NET-8.0-purple)](https://dotnet.microsoft.com/download/dotnet/8.0)
[![Size](https://img.shields.io/badge/size-343%20KB-green)](https://github.com/GabrielxDuO/audio-device-switcher/releases)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

## ✨ 特性

- 🎵 快速切换播放/录制设备
- 💬 独立切换通信设备
- 🌓 暗黑模式菜单（实时响应系统主题切换）
- 📱 高 DPI 支持
- 🚀 开机自启动
- ⚡ 极致轻量：仅 343 KB
- 💾 低内存占用：~15 MB
- 🔋 启动极快：< 0.3 秒

## 🚀 快速开始

### 下载

前往 [Releases](https://github.com/GabrielxDuO/audio-device-switcher/releases) 页面下载最新版本的 `AudioDeviceSwitcher.exe`

### 运行

双击 `AudioDeviceSwitcher.exe` 运行，程序会出现在系统托盘。

### 使用

- **左键/右键点击托盘图标**：打开菜单
- **选择设备**：立即切换
- **开机启动**：勾选菜单中的选项

### 从源码构建

```bash
.\build.bat
```

构建产物位于：`bin\Release\net8.0-windows\win-x64\publish\AudioDeviceSwitcher.exe`

## 📋 系统要求

- Windows 10/11 (64-bit)
- .NET 8.0 Runtime ([下载](https://dotnet.microsoft.com/download/dotnet/8.0))

如果没有安装 .NET Runtime，程序会提示下载安装。

## 📊 性能

| 指标 | 数值 |
|------|------|
| 文件大小 | 343 KB |
| 内存占用 | ~15 MB |
| 启动时间 | < 0.3 秒 |
| CPU 占用 | 接近 0% |

## 🎯 功能说明

### 菜单结构

```
播放设备            ▶
播放通信设备        ▶
录制设备            ▶
录制通信设备        ▶
────────────────────
刷新设备列表
────────────────────
☑ 开机启动
────────────────────
退出
```

## 🛠️ 开发

### 技术栈

- C# 12
- .NET 8
- Windows Forms
- AudioSwitcher.AudioApi.CoreAudio

### 项目结构

```
audio-device-switcher/
├── .github/
│   └── workflows/
│       ├── build.yml          # CI 构建
│       └── release.yml        # 自动发布
├── Program.cs                 # 程序入口
├── TrayApplicationContext.cs  # 主要逻辑
├── AudioDeviceSwitcher.csproj # 项目配置
├── app.manifest               # 应用程序清单
├── icon.ico                   # 亮色模式图标
├── icon-dark.ico              # 暗色模式图标
└── build.bat                  # 构建脚本
```

### CI/CD

项目使用 GitHub Actions 进行持续集成和自动发布：

- **构建工作流** (`build.yml`)：每次推送到 main/master 分支时自动构建
- **发布工作流** (`release.yml`)：推送版本标签时自动创建 Release

#### 创建新版本

```bash
# 1. 更新版本号 (AudioDeviceSwitcher.csproj 和 README.md)
# 2. 提交更改
git add .
git commit -m "bump: version 1.2.0"

# 3. 创建并推送标签
git tag v1.2.0
git push origin v1.2.0

# GitHub Actions 会自动构建并创建 Release
```

## 📄 许可证

MIT License

## 👨‍💻 作者

GabrielxD

---

**从 76 MB (Electron) 到 343 KB (C#) - 体积减少 99.5%！** 🎉

