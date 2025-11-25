# 发布指南

本文档说明如何为 AudioDeviceSwitcher 创建新版本。

## 发布流程

### 1. 更新版本号

在以下文件中更新版本号：

- `AudioDeviceSwitcher.csproj`：
  ```xml
  <Version>1.2.0</Version>
  ```

- `README.md`：
  ```markdown
  ![Version](https://img.shields.io/badge/version-1.2.0-blue)
  ```

### 2. 更新更新日志

在 `CHANGELOG.md` 中添加新版本的更新内容：

```markdown
## [1.2.0] - 2025-11-25

### 新增
- 新功能描述

### 修复
- Bug 修复描述

### 优化
- 优化内容描述
```

### 3. 提交更改

```bash
git add .
git commit -m "bump: version 1.2.0"
git push origin main
```

### 4. 创建并推送标签

```bash
# 创建标签
git tag -a v1.2.0 -m "Release version 1.2.0"

# 推送标签
git push origin v1.2.0
```

### 5. 自动发布

推送标签后，GitHub Actions 会自动：

1. 构建项目
2. 生成 `AudioDeviceSwitcher.exe`
3. 创建 GitHub Release
4. 上传可执行文件

查看进度：访问 [Actions](https://github.com/GabrielxD/audio-device-switcher/actions) 页面

### 6. 验证发布

1. 访问 [Releases](https://github.com/GabrielxD/audio-device-switcher/releases) 页面
2. 确认新版本已创建
3. 下载并测试 `AudioDeviceSwitcher.exe`

## 版本号规范

本项目遵循 [语义化版本](https://semver.org/lang/zh-CN/) 规范：

- **主版本号 (MAJOR)**：不兼容的 API 修改
- **次版本号 (MINOR)**：向下兼容的功能性新增
- **修订号 (PATCH)**：向下兼容的问题修正

示例：
- `1.0.0` → `2.0.0`：重大更新（破坏性变更）
- `1.0.0` → `1.1.0`：功能更新（新增功能）
- `1.0.0` → `1.0.1`：Bug 修复

## 紧急修复

如果需要快速发布紧急修复：

```bash
# 1. 创建修复分支
git checkout -b hotfix/critical-bug

# 2. 修复问题并提交
git add .
git commit -m "fix: critical bug description"

# 3. 合并到主分支
git checkout main
git merge hotfix/critical-bug

# 4. 更新版本号（修订号 +1）
# 例如: 1.2.0 → 1.2.1

# 5. 创建标签并推送
git tag -a v1.2.1 -m "Hotfix: critical bug"
git push origin main
git push origin v1.2.1
```

## 回滚发布

如果发布出现问题，可以删除标签和 Release：

```bash
# 删除本地标签
git tag -d v1.2.0

# 删除远程标签
git push origin :refs/tags/v1.2.0

# 然后在 GitHub 上手动删除 Release
```

## 注意事项

- ✅ 发布前务必在本地测试构建
- ✅ 确保所有测试通过
- ✅ 更新日志必须清晰描述变更
- ✅ 标签格式必须是 `vX.Y.Z`（带 `v` 前缀）
- ❌ 不要删除已发布的稳定版本标签
- ❌ 不要在 Release 工作流运行时推送新标签

