# 发布指南

## 发布流程

### 1. 更新版本号

修改 `CMakeLists.txt` 第 3 行：

```cmake
project(AudioDeviceSwitcher VERSION 2.1.0 ...)
```

修改 `README.md` 中的版本徽章：

```markdown
[![Version](https://img.shields.io/badge/version-2.1.0-blue)](...)
```

### 2. 更新 CHANGELOG.md

在文件顶部添加新版本记录：

```markdown
## [2.1.0] - YYYY-MM-DD

### 新增
- ...

### 修复
- ...

### 变更
- ...
```

### 3. 提交并推送

```bash
git add .
git commit -m "bump: version 2.1.0"
git push origin main
```

### 4. 打标签并发布

```bash
git tag -a v2.1.0 -m "Release v2.1.0"
git push origin v2.1.0
```

推送标签后，GitHub Actions 会自动编译并创建 GitHub Release，附带 `AudioDeviceSwitcher.exe`。

---

## 紧急修复

```bash
git checkout -b hotfix/问题描述
# 修复问题
git add .
git commit -m "fix: 问题描述"
git checkout main
git merge hotfix/问题描述
# 修订号 +1，更新 CHANGELOG
git tag -a v2.0.1 -m "Hotfix: 问题描述"
git push origin main
git push origin v2.0.1
```

## 回滚发布

```bash
# 删除本地和远程标签
git tag -d v2.1.0
git push origin :refs/tags/v2.1.0
# 然后在 GitHub 上手动删除对应的 Release
```

---

## 注意事项

- 标签格式必须为 `vX.Y.Z`
- 推送标签前务必在本地运行 `build.bat` 验证构建
- 必须先更新 CHANGELOG 再推送标签
- 不要删除已被用户下载过的稳定版本标签
- 不要在 Release 工作流运行期间推送新标签
