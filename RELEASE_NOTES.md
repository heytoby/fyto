# fyto 1.0.0.40

Windows 和 macOS 的更新检查优先读取正式 Release 的公开更新清单，避免匿名 GitHub API 额度用尽导致 403。安装包仍需通过版本和 SHA-256 校验。遇到旧版的 403 时，下载本版安装包覆盖升级一次。

Windows 提供标准 MSI，macOS 提供通用 DMG。
