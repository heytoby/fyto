# fyto

轻量的图片浏览器，基于 [void Image Viewer](https://github.com/voidtools/voidImageViewer) 开发，采用 MIT 许可证。

中文界面，极简显示。试图做最极速最轻量的看图软件。
不支持raw格式，所以在有大量raw格式图片时能够快速跳过，无延迟地浏览图片，非常适合专业摄影师选图。

| 按键 | 功能 |
| --- | --- |
| 1 | 显示/隐藏菜单栏，切换标准与精简模式 |
| A | 显示/隐藏 EXIF 侧边栏 |
| Esc | 退出 |
| 鼠标中键 | 切换适应窗口与 100% 大小 |


默认更新仓库为 [heytoby/fyto](https://github.com/heytoby/fyto)。也可在 fyto 设置目录创建 `updates.ini`：

```ini
[updates]
repository=heytoby/fyto
```

更新程序优先读取 GitHub 正式 Release 的公开更新清单，旧发布缺少清单时使用 GitHub API。私有仓库的下载需要先在浏览器登录并具备仓库权限，软件内的更新检查无法读取私有仓库。没有可访问的正式版本、离线或校验失败时，软件保留当前版本。

## 构建与发布

参见 [发布指南](PUBLISHING.zh-CN.md) 和 [构建说明](BUILD-fyto.md)。`scripts/Build-Release.ps1` 生成 Windows MSI、旧版更新器兼容 EXE、ZIP、源码包和 SHA256SUMS。先在构建分支打包并检查 Windows 安装文件，再将已验证的同一批构建产物发布到 GitHub。

macOS 构建使用 Xcode 命令行工具运行 macos/Build-App.sh。

## 来源与许可

原项目：void Image Viewer。原作者：voidtools / David Carpenter。原版权声明及 MIT 全文保留于 [LICENSE](LICENSE)。fyto 是修改发行版；修改包括中文界面、品牌资源、EXIF 侧边栏、窗口行为和更新功能。WebP 的版权和专利声明见 `voidImageViewer/libwebp`，发布包中同时附带对应文件。详见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
