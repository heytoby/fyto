import AppKit
import CryptoKit
import Darwin

private let repository = "heytoby/fyto"
private let bundleID = "io.github.heytoby.fyto"
private let manifestURL = URL(string: "https://github.com/\(repository)/releases/latest/download/fyto-update.json")!
private let latestURL = URL(string: "https://api.github.com/repos/\(repository)/releases/latest")!
private let maxInstallerBytes: Int64 = 128 * 1024 * 1024

struct UpdateFailure: LocalizedError {
    let message: String
    var errorDescription: String? { message }

    init(_ message: String) { self.message = message }
}

struct FytoVersion: Comparable, CustomStringConvertible {
    private let components: [Int]

    init(_ input: String) throws {
        let text = input.hasPrefix("v") ? String(input.dropFirst()) : input
        let parts = text.split(separator: ".", omittingEmptySubsequences: false)
        let numbers = parts.compactMap { Int($0) }
        guard numbers.count == 4, parts.count == 4,
              zip(parts, numbers).allSatisfy({ String($0.1) == $0.0 && (0...65535).contains($0.1) }) else {
            throw UpdateFailure("版本号格式有误。")
        }
        components = numbers
    }

    var description: String { components.map(String.init).joined(separator: ".") }

    static func < (lhs: Self, rhs: Self) -> Bool {
        lhs.components.lexicographicallyPrecedes(rhs.components)
    }
}

struct MacUpdateAsset {
    let version: FytoVersion
    let url: URL
    let size: Int64
    let digest: String
}

struct MacUpdateCheck {
    let installed: FytoVersion
    let latest: FytoVersion
    let asset: MacUpdateAsset?
}

private struct Release: Decodable {
    struct Asset: Decodable {
        let name: String
        let browser_download_url: String
        let state: String
        let size: Int64
        let digest: String?
    }

    let tag_name: String
    let draft: Bool
    let prerelease: Bool
    let assets: [Asset]

    func check(installed: FytoVersion) throws -> MacUpdateCheck {
        guard !draft, !prerelease else { throw UpdateFailure("GitHub 返回的不是正式版本。") }
        let version = try FytoVersion(tag_name)
        guard version > installed else { return MacUpdateCheck(installed: installed, latest: version, asset: nil) }

        let filename = "fyto-\(version)-macos-universal.zip"
        let matches = assets.filter { $0.name == filename && $0.state == "uploaded" }
        guard matches.count == 1, let asset = matches.first else {
            throw UpdateFailure("GitHub 上的 \(version) 正式版尚未提供 macOS 安装包。")
        }
        let expectedURL = "https://github.com/\(repository)/releases/download/\(tag_name)/\(filename)"
        guard asset.browser_download_url == expectedURL, let url = URL(string: expectedURL),
              (1...maxInstallerBytes).contains(asset.size),
              let hash = asset.digest, hash.range(of: "^sha256:[0-9a-fA-F]{64}$", options: .regularExpression) != nil else {
            throw UpdateFailure("Mac 安装包的地址、大小或 SHA-256 校验信息无效。")
        }
        return MacUpdateCheck(installed: installed, latest: version,
                              asset: MacUpdateAsset(version: version, url: url, size: asset.size,
                                                    digest: String(hash.dropFirst(7)).lowercased()))
    }
}

private final class UpdateTransport: NSObject, URLSessionDownloadDelegate, @unchecked Sendable {
    private let limit: Int64

    init(limit: Int64) { self.limit = limit }

    static func allows(_ url: URL) -> Bool {
        guard url.scheme == "https", url.port == nil, url.user == nil, url.password == nil,
              let host = url.host?.lowercased() else { return false }
        switch host {
        case "api.github.com": return url.path == latestURL.path
        case "github.com": return url.path == manifestURL.path || url.path.hasPrefix("/\(repository)/releases/download/")
        case "release-assets.githubusercontent.com", "objects.githubusercontent.com": return true
        default: return false
        }
    }

    func urlSession(_ session: URLSession, task: URLSessionTask, willPerformHTTPRedirection response: HTTPURLResponse,
                    newRequest request: URLRequest, completionHandler: @escaping (URLRequest?) -> Void) {
        completionHandler(request.url.map(Self.allows) == true ? request : nil)
    }

    func urlSession(_ session: URLSession, downloadTask: URLSessionDownloadTask, didFinishDownloadingTo location: URL) {}

    func urlSession(_ session: URLSession, downloadTask: URLSessionDownloadTask, didWriteData bytesWritten: Int64,
                    totalBytesWritten: Int64, totalBytesExpectedToWrite: Int64) {
        if totalBytesWritten > limit || totalBytesExpectedToWrite > limit { downloadTask.cancel() }
    }

    func makeSession() -> URLSession {
        let configuration = URLSessionConfiguration.ephemeral
        configuration.timeoutIntervalForRequest = 45
        configuration.timeoutIntervalForResource = 300
        configuration.httpAdditionalHeaders = ["User-Agent": "fyto-macos-updater", "Accept-Encoding": "identity"]
        return URLSession(configuration: configuration, delegate: self, delegateQueue: nil)
    }
}

enum MacUpdater {
    private static let fileManager = FileManager.default

    static func check() async throws -> MacUpdateCheck {
        let installed = try version(of: Bundle.main.bundleURL)
        let session = UpdateTransport(limit: 2 * 1024 * 1024).makeSession()
        defer { session.finishTasksAndInvalidate() }
        var (bytes, response) = try await session.bytes(for: URLRequest(url: manifestURL))
        if let missing = response as? HTTPURLResponse, missing.statusCode == 404,
           let url = missing.url, UpdateTransport.allows(url) {
            var request = URLRequest(url: latestURL)
            request.setValue("application/vnd.github+json", forHTTPHeaderField: "Accept")
            (bytes, response) = try await session.bytes(for: request)
        }
        try validateResponse(response)
        var data = Data()
        for try await byte in bytes {
            guard data.count < 2 * 1024 * 1024 else { throw UpdateFailure("版本信息过大。") }
            data.append(byte)
        }
        return try JSONDecoder().decode(Release.self, from: data).check(installed: installed)
    }

    @MainActor
    static func install(_ asset: MacUpdateAsset, replacing current: FytoVersion, reopen photo: URL?) async throws {
        let installed = Bundle.main.bundleURL.standardizedFileURL
        guard installed.pathExtension.lowercased() == "app",
              !installed.path.contains("/AppTranslocation/"),
              fileManager.isWritableFile(atPath: installed.deletingLastPathComponent().path) else {
            throw UpdateFailure("当前应用位置无法写入。请把 fyto.app 移到可写的应用程序文件夹后重试。")
        }

        let cache = try fileManager.url(for: .cachesDirectory, in: .userDomainMask, appropriateFor: nil, create: true)
            .appendingPathComponent(bundleID, isDirectory: true)
        try fileManager.createDirectory(at: cache, withIntermediateDirectories: true, attributes: [.posixPermissions: 0o700])
        let lock = Darwin.open(cache.appendingPathComponent("update.lock").path,
                               O_CREAT | O_RDWR | O_NOFOLLOW | O_CLOEXEC, 0o600)
        guard lock >= 0 else { throw UpdateFailure("无法建立更新锁。") }
        defer { Darwin.close(lock) }
        guard flock(lock, LOCK_EX | LOCK_NB) == 0 else { throw UpdateFailure("另一个 fyto 窗口正在安装更新。") }
        guard try version(of: installed) == current else { throw UpdateFailure("磁盘上的 fyto 版本已改变，请重新打开应用。") }

        let staging = try fileManager.url(for: .itemReplacementDirectory, in: .userDomainMask,
                                          appropriateFor: installed, create: true)
        var preserveBackup = false
        defer {
            if !preserveBackup { try? fileManager.removeItem(at: staging) }
        }

        let archive = staging.appendingPathComponent("update.zip")
        let session = UpdateTransport(limit: asset.size).makeSession()
        defer { session.finishTasksAndInvalidate() }
        let (download, response) = try await session.download(from: asset.url)
        try validateResponse(response)
        try fileManager.moveItem(at: download, to: archive)

        let replacement = try await Task.detached(priority: .userInitiated) {
            let actual = try archive.resourceValues(forKeys: [.fileSizeKey]).fileSize
            guard actual.map(Int64.init) == asset.size, try sha256(archive) == asset.digest else {
                throw UpdateFailure("Mac 安装包的大小或 SHA-256 校验失败。")
            }
            return try validateArchive(archive, in: staging, expected: asset.version)
        }.value

        guard try version(of: installed) == current else { throw UpdateFailure("磁盘上的 fyto 版本已改变，请重新打开应用。") }
        try exchange(installed, replacement)
        do {
            let launch = NSWorkspace.OpenConfiguration()
            launch.createsNewApplicationInstance = true
            launch.allowsRunningApplicationSubstitution = false
            launch.addsToRecentItems = false
            if let photo { launch.arguments = [photo.path] }
            let next = try await NSWorkspace.shared.openApplication(at: installed, configuration: launch)
            guard next.processIdentifier != ProcessInfo.processInfo.processIdentifier else {
                throw UpdateFailure("系统没有启动新版 fyto。")
            }
        } catch {
            do { try exchange(installed, replacement) }
            catch {
                preserveBackup = true
                throw UpdateFailure("新版启动失败。原版保存在 \(replacement.path)。")
            }
            throw UpdateFailure("新版启动失败，已恢复原版。")
        }
    }

    private static func validateResponse(_ response: URLResponse) throws {
        guard let http = response as? HTTPURLResponse, let url = http.url, UpdateTransport.allows(url) else {
            throw UpdateFailure("更新服务器返回了不受信任的地址。")
        }
        if http.statusCode == 429 ||
           (http.statusCode == 403 && (http.value(forHTTPHeaderField: "X-RateLimit-Remaining") == "0" ||
                                      http.value(forHTTPHeaderField: "Retry-After") != nil)) {
            throw UpdateFailure("GitHub 暂时限制了更新请求，请稍后再试。")
        }
        guard http.statusCode == 200 else {
            throw UpdateFailure("无法从 GitHub 获取更新，HTTP \(http.statusCode)。")
        }
    }

    private static func sha256(_ file: URL) throws -> String {
        let reader = try FileHandle(forReadingFrom: file)
        defer { try? reader.close() }
        var hasher = SHA256()
        while let bytes = try reader.read(upToCount: 256 * 1024), !bytes.isEmpty { hasher.update(data: bytes) }
        return hasher.finalize().map { String(format: "%02x", $0) }.joined()
    }

    private static func version(of app: URL) throws -> FytoVersion {
        let plist = app.appendingPathComponent("Contents/Info.plist")
        let data = try Data(contentsOf: plist)
        guard let info = try PropertyListSerialization.propertyList(from: data, format: nil) as? [String: Any],
              info["CFBundleIdentifier"] as? String == bundleID, info["CFBundleExecutable"] as? String == "fyto",
              let version = info["FYTOFullVersion"] as? String else {
            throw UpdateFailure("安装包的应用标识或版本号无效。")
        }
        return try FytoVersion(version)
    }

    private static func validateArchive(_ archive: URL, in staging: URL, expected: FytoVersion) throws -> URL {
        let extraction = staging.appendingPathComponent("expanded", isDirectory: true)
        try fileManager.createDirectory(at: extraction, withIntermediateDirectories: false,
                                        attributes: [.posixPermissions: 0o700])
        // The system tar rejects parent traversal and symlinks that escape its extraction directory.
        try run("/usr/bin/tar", ["-x", "-f", archive.path, "-C", extraction.path], "无法解压 Mac 安装包。")
        let app = extraction.appendingPathComponent("fyto.app", isDirectory: true)
        let keys: Set<URLResourceKey> = [.isSymbolicLinkKey, .isDirectoryKey, .isRegularFileKey]
        let root = try app.resourceValues(forKeys: keys)
        guard root.isDirectory == true, root.isSymbolicLink == false,
              let files = fileManager.enumerator(at: app, includingPropertiesForKeys: Array(keys)) else {
            throw UpdateFailure("Mac 安装包缺少 fyto.app。")
        }
        for case let path as URL in files {
            let item = try path.resourceValues(forKeys: keys)
            guard item.isSymbolicLink == false, item.isDirectory == true || item.isRegularFile == true else {
                throw UpdateFailure("Mac 安装包包含异常的文件项。")
            }
        }
        guard try version(of: app) == expected,
              fileManager.isExecutableFile(atPath: app.appendingPathComponent("Contents/MacOS/fyto").path) else {
            throw UpdateFailure("Mac 安装包的版本或可执行文件不匹配。")
        }
        try run("/usr/bin/codesign", ["--verify", "--deep", "--strict", "--all-architectures", app.path],
                "Mac 安装包的代码签名完整性验证失败。")
        return app
    }

    private static func run(_ executable: String, _ arguments: [String], _ failure: String) throws {
        let process = Process()
        process.executableURL = URL(fileURLWithPath: executable)
        process.arguments = arguments
        process.standardOutput = FileHandle.nullDevice
        process.standardError = FileHandle.nullDevice
        try process.run()
        process.waitUntilExit()
        guard process.terminationStatus == 0 else { throw UpdateFailure(failure) }
    }

    private static func exchange(_ a: URL, _ b: URL) throws {
        guard renameatx_np(AT_FDCWD, a.path, AT_FDCWD, b.path, UInt32(RENAME_SWAP)) == 0 else {
            let error = NSError(domain: NSPOSIXErrorDomain, code: Int(errno))
            throw UpdateFailure("无法替换 fyto.app，\(error.localizedDescription)")
        }
    }

    static func verifyFeed() throws {
        let installed = try FytoVersion("1.0.0.36")
        let digest = String(repeating: "a", count: 64)
        let asset = Release.Asset(name: "fyto-1.0.0.37-macos-universal.zip",
                                  browser_download_url: "https://github.com/\(repository)/releases/download/v1.0.0.37/fyto-1.0.0.37-macos-universal.zip",
                                  state: "uploaded", size: 123, digest: "sha256:" + digest)
        let candidate = try Release(tag_name: "v1.0.0.37", draft: false, prerelease: false, assets: [asset]).check(installed: installed)
        precondition(candidate.asset?.digest == digest)
        let old = try Release(tag_name: "v1.0.0.33", draft: false, prerelease: false, assets: []).check(installed: installed)
        precondition(old.asset == nil && old.latest < installed)
        precondition(!UpdateTransport.allows(URL(string: "https://github.com.evil.test/heytoby/fyto/releases/download/a")!))
        precondition(UpdateTransport.allows(manifestURL))
        precondition(!UpdateTransport.allows(URL(string: "https://github.com/another/project/releases/latest/download/fyto-update.json")!))
        precondition(!UpdateTransport.allows(URL(string: "https://github.com/another/project/releases/download/a")!))
        print("macOS updater release parsing, version, hash and trusted hosts verified")
    }

    static func verifyArchive(_ archive: URL) throws {
        let expected = try version(of: Bundle.main.bundleURL)
        let work = fileManager.temporaryDirectory.appendingPathComponent("fyto-update-test-\(UUID())", isDirectory: true)
        try fileManager.createDirectory(at: work, withIntermediateDirectories: false, attributes: [.posixPermissions: 0o700])
        defer { try? fileManager.removeItem(at: work) }
        let replacement = try validateArchive(archive, in: work, expected: expected)
        let installed = work.appendingPathComponent("installed.app", isDirectory: true)
        try fileManager.createDirectory(at: installed, withIntermediateDirectories: false)
        let marker = installed.appendingPathComponent("previous-version")
        try Data([1]).write(to: marker)
        try exchange(installed, replacement)
        let updated = try version(of: installed)
        precondition(updated == expected)
        try exchange(installed, replacement)
        precondition(fileManager.fileExists(atPath: marker.path))
        print("macOS update archive, signature, atomic replacement and rollback verified")
    }
}