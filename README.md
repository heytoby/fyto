# fyto

轻量的 Windows 图片浏览器，基于 [void Image Viewer](https://github.com/voidtools/voidImageViewer) 开发，采用 MIT 许可证。

支持中文界面、EXIF 侧边栏、标准/极简模式、透明图片棋盘背景和多显示器窗口适配。

## 安装和使用

下载 Release 中的 `fyto-Setup-版本号.exe` 并运行；也可以解压同名 ZIP 后运行其中的安装程序。支持 Windows 10/11 x64。升级会沿用原设置，安装前自动关闭当前 Windows 会话中正在运行的 fyto。首次安装可在软件中选择图片格式。

安装完成后的空窗口保持 16:10 横向比例，在竖屏显示器上也不会变成竖长窗口。从文件管理器打开图片时，默认在 Windows 主显示器全屏显示，图片文件夹或鼠标所在的屏幕不会影响打开位置。已运行的 fyto 接收新图片时也会移至主显示器；手动拖动窗口后，在软件内翻看上一张、下一张会沿用当前位置。普通窗口浏览时按图片比例适配，分屏/最大化/全屏保留固定窗口区域。画面比例不同产生的必要留白会保留，图片不拉伸。

| 按键 | 功能 |
| --- | --- |
| 1 | 显示/隐藏菜单栏，切换标准与极简模式 |
| A | 显示/隐藏 EXIF 侧边栏 |
| Esc | 退出 |
| 鼠标中键 | 切换适应窗口与 100% 大小 |

全屏时拖动标题栏会恢复普通窗口并继续移动；极简模式可拖动窗口顶部区域。单击或轻微晃动不退出全屏。分屏、最大化窗口拖动时也会恢复普通窗口，随后使用 Windows 原生移动与边缘吸附。

## 手动更新

点击 **帮助 → 检查更新**。软件不会在启动时或后台检查，也不会主动弹出更新提示。发现更高版本时，自动下载安装包、校验 SHA-256、启动覆盖安装并重新打开 fyto。安装需要允许 Windows 管理员权限提示；取消权限提示会中止更新。更新检查需要 .NET Framework 4.8、网络连接和可访问的 GitHub 正式 Release。

默认更新仓库为 [heytoby/fyto](https://github.com/heytoby/fyto)。也可在 fyto 设置目录创建 `updates.ini`：

```ini
[updates]
repository=heytoby/fyto
```

更新程序通过 GitHub 的公开接口读取正式 Release。私有仓库的下载需要先在浏览器登录并具备仓库权限，软件内的更新检查无法读取私有仓库。没有可访问的正式版本、离线或校验失败时，软件保留当前版本。

## 构建与发布

参见 [发布指南](PUBLISHING.zh-CN.md) 和 [构建说明](BUILD-fyto.md)。`scripts/Build-Release.ps1` 生成带版本号的安装 EXE、ZIP、源码包及 SHA256SUMS。更新 main 分支的版本号或推送版本标签后，GitHub 工作流构建并校验安装包，再发布正式 Release。

## 来源与许可

原项目：void Image Viewer。原作者：voidtools / David Carpenter。原版权声明及 MIT 全文保留于 [LICENSE](LICENSE)。fyto 是修改发行版；修改包括中文界面、品牌资源、EXIF 侧边栏、窗口行为和更新功能。WebP 的版权和专利声明见 `voidImageViewer/libwebp`，发布包中同时附带对应文件。详见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
