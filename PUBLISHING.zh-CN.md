# fyto 构建与发布

项目仓库为 https://github.com/heytoby/fyto。已发布的正式版本供应用内更新读取。

## 本地构建

Windows 构建需要 Visual Studio 2022 C++ Build Tools、Windows SDK、.NET SDK 和 .NET Framework 4.8，安装包使用 WiX 6.0.2。首次构建会下载并校验 Adobe 思源黑体。运行 `scripts/Build-Release.ps1` 后，在 release-assets 中取得 MSI、兼容旧版更新器的 EXE、安装 ZIP、源码包及 SHA256SUMS.txt。Mac 构建使用 Xcode 命令行工具执行 `macos/Build-App.sh`。

## GitHub 发布

1. 用 `scripts/Set-Version.ps1` 更新版本，编写发布说明，将源码提交至 `build/fyto-版本号`。GitHub Actions 分别构建 Windows 和 macOS，验证安装包并上传产物。
2. 下载成功运行的 Windows artifact ZIP。在启用 Defender 的 Windows 机器上执行 `python scripts/Verify-WindowsRelease.py <artifact.zip> <version>`。扫描 MSI 和旧版更新所用 EXE，记录脚本输出的两个 SHA-256。可传入 `--control` 对此前触发告警的正式包复现检测。
3. 在已打包的提交之后创建一个空提交，提交消息尾部写入 `Fyto-Build-Run`、`Fyto-MSI-SHA256` 和 `Fyto-EXE-SHA256` 三个 trailer。将该提交快进到 main。发布工作流复用上述运行的产物，逐个校验，再发布 GitHub Release。版本必须递增。

Windows 从 MSI 安装。1.0.0.39 及以上版本的更新器优先使用 MSI。兼容 EXE 供仍依赖 EXE 的旧版更新器使用。发布工作流根据已校验的 Windows MSI、兼容 EXE 和 macOS ZIP 生成 `fyto-update.json`，上传后验证匿名下载地址返回同一份清单。

## 来源与许可

fyto 基于 void Image Viewer，原作者为 voidtools / David Carpenter。发布时保留 LICENSE、voidImageViewer/LICENSE、THIRD_PARTY_NOTICES.md 和 WebP 的 COPYING、PATENTS。
