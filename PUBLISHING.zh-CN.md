# fyto 构建与发布

项目仓库为 https://github.com/heytoby/fyto，当前版本为 1.0.0.34，标签为 v1.0.0.34。仓库已公开，正式发布可供软件内更新读取。

## 本地构建

需要 Windows x64、Visual Studio 2022 C++ Build Tools、Windows SDK、.NET SDK 和 .NET Framework 4.8。构建脚本使用 WiX 6.0.2 及同版本 UI 扩展。从 GitHub 检出的源码首次构建时会下载并校验 Adobe 的思源黑体 2.005R，发布源码包已含字体和许可。

在 PowerShell 中运行：

```powershell
./scripts/Set-Version.ps1 -Version 1.0.0.34
./scripts/Build-Release.ps1
```

release-assets 目录中会生成带版本号的安装 EXE、安装 ZIP、源码 ZIP 和 SHA256SUMS.txt。构建脚本执行安装包内置的 /verify 检查，并在出错时停止。源码包排除编译产物和本机设置。

## GitHub 发布

提交完整源码到 main 分支并更新 version.h 中的版本号，工作流会构建安装包并发布正式 Release。也可推送与源码版本一致的 v1.0.0.BUILD 标签，或手动运行工作流。已存在的 Release 不会被替换。

保持 MSI UpgradeCode、关联 GUID 和用户设置位置不变。发布版本号必须递增，不用相同标签替换不同内容。软件的手动更新检查要求正式 Release、独立的安装 EXE 附件以及 GitHub 提供的 SHA-256 digest。

## 仓库权限与更新

私有仓库内的源码和 Release 仅对有权限的账号开放。当前更新程序使用 GitHub 公开接口，未嵌入访问令牌，无法读取私有仓库。要向所有人提供直接下载及软件内更新，需要由仓库所有者将仓库设为公开。

## 来源与许可

fyto 基于 void Image Viewer 开发，原作者为 voidtools / David Carpenter。保留 LICENSE、voidImageViewer/LICENSE、THIRD_PARTY_NOTICES.md，以及 WebP 的 COPYING 和 PATENTS。安装包同时包含相应声明。MIT 允许修改及分发，分发时需保留版权及许可声明。

## 发布核验

核对源码、程序及安装包的版本号一致，安装包内置验证通过，ZIP 包含许可文件，SHA256SUMS.txt 与三个附件一致。正式发布后核对四个附件完整。在线覆盖升级需要可访问的较新 Release，当前本地构建验证不代替在线升级测试。
