import AppKit
import ImageIO
import UniformTypeIdentifiers

func viewerFont(_ size: CGFloat) -> NSFont { NSFont.systemFont(ofSize: size) }

let titleHeight: CGFloat = 36
let supportedExtensions = Set(["jpg", "jpeg", "png", "gif", "bmp", "tif", "tiff",
                               "webp", "heic", "heif", "ico", "avif"])

func imageFit(_ size: CGSize, in limit: CGSize, longEdge: CGFloat = .greatestFiniteMagnitude) -> CGSize {
    guard size.width > 0, size.height > 0, limit.width > 0, limit.height > 0 else { return .zero }
    let scale = min(limit.width / size.width, limit.height / size.height,
                    longEdge / max(size.width, size.height))
    return CGSize(width: max(1, min(limit.width, longEdge, ceil(size.width * scale))),
                  height: max(1, min(limit.height, longEdge, ceil(size.height * scale))))
}

struct Photo {
    let url: URL
    let image: NSImage
    let pixels: CGSize
    let metadata: PhotoMetadata

    init?(url: URL) {
        guard let source = CGImageSourceCreateWithURL(url as CFURL, nil),
              let properties = CGImageSourceCopyPropertiesAtIndex(source, 0, nil) as? [CFString: Any],
              let width = properties[kCGImagePropertyPixelWidth] as? NSNumber,
              let height = properties[kCGImagePropertyPixelHeight] as? NSNumber,
              width.doubleValue > 0, height.doubleValue > 0,
              let image = NSImage(contentsOf: url), image.isValid else { return nil }
        let orientation = (properties[kCGImagePropertyOrientation] as? NSNumber)?.intValue ?? 1
        let swapped = (5...8).contains(orientation)
        self.url = url
        self.image = image
        let dimensions = CGSize(width: swapped ? height.doubleValue : width.doubleValue,
                                height: swapped ? width.doubleValue : height.doubleValue)
        self.pixels = dimensions
        self.metadata = PhotoMetadata(properties: properties, dimensions: dimensions)
    }

    func size(turned turns: Int) -> CGSize {
        turns.isMultiple(of: 2) ? pixels : CGSize(width: pixels.height, height: pixels.width)
    }

    func displayImage(turned turns: Int) -> NSImage {
        guard turns != 0 else { return image }
        let source = image, original = pixels
        return NSImage(size: size(turned: turns), flipped: false) { rect in
            NSGraphicsContext.saveGraphicsState()
            let transform = NSAffineTransform()
            transform.translateX(by: rect.midX, yBy: rect.midY)
            transform.rotate(byDegrees: -CGFloat(turns * 90))
            transform.concat()
            let target = NSRect(x: -original.width / 2, y: -original.height / 2,
                                width: original.width, height: original.height)
            source.draw(in: target, from: .zero, operation: .sourceOver, fraction: 1)
            NSGraphicsContext.restoreGraphicsState()
            return true
        }
    }
}

final class ViewerWindow: NSWindow {
    var handleKey: ((NSEvent) -> Bool)?

    override var canBecomeKey: Bool { true }
    override var canBecomeMain: Bool { true }

    override func sendEvent(_ event: NSEvent) {
        if event.type == .keyDown, handleKey?(event) == true { return }
        super.sendEvent(event)
    }
}

@MainActor
final class Fyto: NSObject, NSApplicationDelegate, NSWindowDelegate {
    let window = ViewerWindow(contentRect: NSRect(x: 0, y: 0, width: 800, height: 500),
                              styleMask: [.borderless, .resizable], backing: .buffered, defer: false)
    let canvas = ViewerCanvas()
    private(set) var standard = UserDefaults.standard.bool(forKey: "standardMode")
    private(set) var pinned = UserDefaults.standard.bool(forKey: "pinned")
    private(set) var photo: Photo?
    private(set) var metadataOpen = false
    private var files: [URL] = []
    private var index = 0
    private var turns = 0
    private var slideshow: Timer?
    private var manualLongEdge: CGFloat?
    private var expanded = false
    private var restoreFrame: NSRect?
    private var scrollAmount: CGFloat = 0
    private var updating = false

    var playing: Bool { slideshow != nil }
    var showsMetadata: Bool { metadataOpen && photo != nil }

    func metadataWidth(in width: CGFloat) -> CGFloat {
        showsMetadata ? min(320, max(0, width - 100)) : 0
    }
    var screen: NSScreen? { window.screen ?? NSScreen.screens.first }

    override init() {
        super.init()
        canvas.viewer = self
        window.contentView = canvas
        window.delegate = self
        window.isReleasedWhenClosed = false
        window.isOpaque = true
        window.backgroundColor = NSColor(white: 0.11, alpha: 1)
        window.hasShadow = false
        window.acceptsMouseMovedEvents = true
        window.minSize = NSSize(width: 64, height: 64)
        window.title = "fyto"
        window.handleKey = { [weak self] in self?.key($0) ?? false }
        applyPin()
    }

    func applicationDidFinishLaunching(_ notification: Notification) {
        let paths = CommandLine.arguments.dropFirst().filter { !$0.hasPrefix("-") }
        if !paths.isEmpty { open(paths.map { URL(fileURLWithPath: $0) }) }
        if photo == nil { fitWindow(on: NSScreen.screens.first) }
        showWindow()
    }

    func application(_ sender: NSApplication, openFiles filenames: [String]) {
        open(filenames.map { URL(fileURLWithPath: $0) })
        sender.reply(toOpenOrPrint: .success)
    }

    func applicationShouldHandleReopen(_ sender: NSApplication, hasVisibleWindows: Bool) -> Bool {
        showWindow()
        return false
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }

    func applicationShouldTerminate(_ sender: NSApplication) -> NSApplication.TerminateReply {
        updating ? .terminateCancel : .terminateNow
    }

    func windowDidEndLiveResize(_ notification: Notification) {
        expanded = false
        let size = canvas.photoFrame.size
        if size.width > 0, size.height > 0 { manualLongEdge = max(size.width, size.height) }
    }

    func showWindow() {
        window.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
    }

    @objc func openDialog() {
        let panel = NSOpenPanel()
        panel.allowedContentTypes = [.image]
        panel.allowsMultipleSelection = true
        panel.canChooseDirectories = false
        panel.begin { [weak self] result in
            if result == .OK { self?.open(panel.urls) }
        }
    }

    func open(_ urls: [URL]) {
        let chosen = urls.filter { supportedExtensions.contains($0.pathExtension.lowercased()) }
        guard let first = chosen.first else { return }
        guard let opened = Photo(url: first) else {
            let alert = NSAlert()
            alert.messageText = "无法打开图片"
            alert.informativeText = first.lastPathComponent
            alert.addButton(withTitle: "确定")
            alert.runModal()
            return
        }

        if chosen.count == 1 {
            let folder = first.deletingLastPathComponent()
            files = ((try? FileManager.default.contentsOfDirectory(at: folder, includingPropertiesForKeys: nil,
                                                                     options: .skipsHiddenFiles)) ?? [])
                .filter { supportedExtensions.contains($0.pathExtension.lowercased()) }
                .sorted { $0.lastPathComponent.localizedStandardCompare($1.lastPathComponent) == .orderedAscending }
            if let found = files.firstIndex(of: first) { index = found }
            else { files = [first]; index = 0 }
        } else {
            files = chosen
            index = 0
        }
        display(opened, opening: true)
        showWindow()
    }

    private func display(_ next: Photo, opening: Bool = false) {
        photo = next
        turns = 0
        canvas.setPhoto(next.image, pixels: next.pixels, name: next.url.lastPathComponent, resetZoom: true)
        canvas.showMetadata(next.metadata)
        window.title = next.url.lastPathComponent
        window.representedURL = next.url
        if !expanded { fitWindow(on: opening ? NSScreen.screens.first : screen) }
    }

    func navigate(_ direction: Int) {
        guard files.count > 1 else { return }
        var target = index
        for _ in 1..<files.count {
            target = (target + direction + files.count) % files.count
            if let next = Photo(url: files[target]) {
                index = target
                display(next)
                return
            }
        }
    }

    @objc func next() { navigate(1) }
    @objc func previous() { navigate(-1) }

    @objc func togglePin() {
        pinned.toggle()
        UserDefaults.standard.set(pinned, forKey: "pinned")
        applyPin()
    }

    private func applyPin() {
        window.level = pinned ? .floating : .normal
        canvas.refreshControls()
    }

    @objc func toggleMetadata() {
        guard photo != nil else { return }
        metadataOpen.toggle()
        window.minSize = NSSize(width: metadataOpen ? 240 : 64, height: metadataOpen ? 160 : 64)
        canvas.refreshControls()
        if !expanded { fitWindow(on: screen) }
    }

    @objc func toggleMode() {
        let previous = window.frame
        canvas.layoutSubtreeIfNeeded()
        let oldPhoto = window.convertToScreen(canvas.convert(canvas.photoFrame, to: nil))
        standard.toggle()
        UserDefaults.standard.set(standard, forKey: "standardMode")
        canvas.refreshControls()
        guard let photo, let screen, canvas.zoom == 1, !oldPhoto.isEmpty else { return }
        let target = modeFrame(previous: previous, picture: oldPhoto, pixels: photo.size(turned: turns),
                               work: screen.visibleFrame, standard: standard)
        if target != previous { window.setFrame(target, display: true) }
    }

    private func modeFrame(previous: NSRect, picture: NSRect, pixels: CGSize, work: NSRect,
                           standard: Bool) -> NSRect {
        let chrome = standard ? titleHeight : 0
        let fillsHeight = previous.height >= work.height - 2
        let fillsScreen = expanded || fillsHeight || previous.width >= work.width - 2
        let outerWidth = expanded || (!standard && fillsHeight) ? work.width : min(work.width, previous.width)
        let panel = metadataWidth(in: outerWidth)
        let available = fillsScreen
            ? CGSize(width: outerWidth - panel, height: min(work.height, expanded ? work.height : previous.height) - chrome)
            : CGSize(width: min(picture.width, work.width - panel), height: min(picture.height, work.height - chrome))
        let fit = imageFit(pixels, in: available)
        guard fit.width > 0, fit.height > 0 else { return previous }
        let width = max(window.minSize.width, fit.width + (fit.width >= 100 ? panel : 0))
        let height = max(window.minSize.height, fit.height + chrome)
        // Window coordinates grow upward. The title occupies space above the image.
        return clamp(NSRect(x: picture.midX - (width - metadataWidth(in: width)) / 2,
                            y: picture.midY - (height - chrome) / 2,
                            width: width, height: height), to: work)
    }

    func verifyModeFitting() {
        precondition(!showsMetadata)
        let work = NSRect(x: 0, y: 0, width: 1920, height: 1040)
        let previous = NSRect(x: 100, y: 100, width: 1200, height: 800)
        let picture = NSRect(x: 450, y: 108, width: 500, height: 750)
        let minimal = modeFrame(previous: previous, picture: picture, pixels: picture.size, work: work, standard: false)
        precondition(minimal == picture)
        let standard = modeFrame(previous: minimal, picture: picture, pixels: picture.size, work: work, standard: true)
        precondition(standard.origin == picture.origin && standard.size == CGSize(width: 500, height: 750 + titleHeight))

        let fitted = modeFrame(previous: work, picture: work, pixels: work.size, work: work, standard: true)
        precondition(fitted.height == work.height && fitted.width < work.width)
        let fittedPicture = NSRect(x: fitted.minX, y: fitted.minY, width: fitted.width, height: fitted.height - titleHeight)
        precondition(modeFrame(previous: fitted, picture: fittedPicture, pixels: work.size, work: work, standard: false) == work)
        print("macOS mode switching preserves the image position and fits screen edges")
    }

    @objc func togglePlayback() {
        if let slideshow {
            slideshow.invalidate()
            self.slideshow = nil
        } else if photo != nil {
            let timer = Timer(timeInterval: 5, target: self, selector: #selector(slideshowTick(_:)),
                              userInfo: nil, repeats: true)
            RunLoop.main.add(timer, forMode: .common)
            slideshow = timer
        }
        canvas.refreshControls()
    }

    @objc private func slideshowTick(_ timer: Timer) { navigate(1) }

    @objc func clockwise() { rotate(1) }
    @objc func counterclockwise() { rotate(-1) }

    private func rotate(_ direction: Int) {
        guard let photo else { return }
        turns = (turns + direction + 4) % 4
        canvas.setPhoto(photo.displayImage(turned: turns), pixels: photo.size(turned: turns),
                        name: photo.url.lastPathComponent, resetZoom: true)
        if !expanded { fitWindow(on: screen) }
    }

    @objc func hideWindow() { window.orderOut(nil) }
    @objc func closeWindow() { if !updating { window.close() } }

    @objc func toggleMaximized() {
        guard let screen else { return }
        if expanded {
            expanded = false
            if let restoreFrame { window.setFrame(clamp(restoreFrame, to: screen.visibleFrame), display: true) }
            else { fitWindow(on: screen) }
        } else {
            restoreFrame = window.frame
            expanded = true
            window.setFrame(screen.visibleFrame, display: true)
        }
    }

    @objc func showInFinder() {
        if let photo { NSWorkspace.shared.activateFileViewerSelecting([photo.url]) }
    }

    func dragWindow(with event: NSEvent) {
        if let screen, expanded || abs(window.frame.height - screen.visibleFrame.height) <= 2
            || abs(window.frame.width - screen.visibleFrame.width) <= 2 {
            let cursor = NSEvent.mouseLocation
            let fraction = min(1, max(0, (cursor.x - window.frame.minX) / max(1, window.frame.width)))
            expanded = false
            manualLongEdge = nil
            canvas.resetZoom()
            fitWindow(on: screen)
            let frame = window.frame
            let next = NSRect(x: cursor.x - frame.width * fraction, y: cursor.y - frame.height + 14,
                              width: frame.width, height: frame.height)
            window.setFrame(clamp(next, to: screen.visibleFrame), display: true)
        }
        window.performDrag(with: event)
    }

    private func fitWindow(on targetScreen: NSScreen?) {
        guard let screen = targetScreen ?? NSScreen.screens.first else { return }
        let work = screen.visibleFrame
        let size: CGSize
        if let photo {
            let chrome = standard ? titleHeight : 0
            let sidebar = metadataWidth(in: work.width)
            let imageArea = CGSize(width: work.width - sidebar, height: work.height - chrome)
            let longEdge = min(manualLongEdge ?? 1980 / screen.backingScaleFactor, imageArea.width, imageArea.height)
            let fitted = imageFit(photo.size(turned: turns), in: imageArea, longEdge: longEdge)
            size = CGSize(width: max(window.minSize.width, fitted.width + sidebar),
                          height: max(window.minSize.height, fitted.height + chrome))
        } else {
            size = imageFit(CGSize(width: 800, height: 500),
                            in: CGSize(width: work.width * 0.9, height: work.height * 0.8), longEdge: 800)
        }
        let center = screen.frame.intersects(window.frame) && photo != nil
            ? CGPoint(x: window.frame.midX, y: window.frame.midY) : CGPoint(x: work.midX, y: work.midY)
        window.setFrame(clamp(NSRect(x: center.x - size.width / 2, y: center.y - size.height / 2,
                                     width: size.width, height: size.height), to: work), display: true)
    }

    private func clamp(_ frame: NSRect, to area: NSRect) -> NSRect {
        let w = min(frame.width, area.width), h = min(frame.height, area.height)
        return NSRect(x: max(area.minX, min(frame.minX, area.maxX - w)),
                      y: max(area.minY, min(frame.minY, area.maxY - h)), width: w, height: h)
    }

    func scroll(_ event: NSEvent) {
        guard photo != nil else { return }
        if event.modifierFlags.contains(.control) {
            canvas.magnify(by: pow(1.015, event.scrollingDeltaY))
            return
        }
        let delta = abs(event.scrollingDeltaY) >= abs(event.scrollingDeltaX)
            ? event.scrollingDeltaY : event.scrollingDeltaX
        scrollAmount += event.hasPreciseScrollingDeltas ? delta : delta * 24
        if abs(scrollAmount) >= 24 {
            navigate(scrollAmount > 0 ? -1 : 1)
            scrollAmount = 0
        }
        if event.phase == .ended || event.momentumPhase == .ended { scrollAmount = 0 }
    }

    func key(_ event: NSEvent) -> Bool {
        let modifiers = event.modifierFlags.intersection([.command, .control, .option, .shift])
        if modifiers == .control {
            switch event.keyCode {
            case 123: if !event.isARepeat { counterclockwise() }; return true
            case 124: if !event.isARepeat { clockwise() }; return true
            default: return false
            }
        }
        let character = event.charactersIgnoringModifiers?.lowercased()
        if modifiers == .command {
            switch character {
            case "o": openDialog(); return true
            case "m": hideWindow(); return true
            case "w": closeWindow(); return true
            case "q": NSApp.terminate(nil); return true
            default: return false
            }
        }
        guard modifiers.isEmpty else { return false }
        switch event.keyCode {
        case 123: previous(); return true
        case 124: next(); return true
        case 53: closeWindow(); return true
        default: break
        }
        switch character {
        case "1": if !event.isARepeat { toggleMode() }; return true
        case "p": if !event.isARepeat { togglePlayback() }; return true
        case "a": if !event.isARepeat { toggleMetadata() }; return true
        case "f": if !event.isARepeat { toggleMaximized() }; return true
        case "0": canvas.resetZoom(); return true
        default: return false
        }
    }

    @objc func checkForUpdates() {
        guard !updating else { return }
        updating = true
        let status = NSTextField(labelWithString: "正在检查 GitHub 正式版…")
        status.font = viewerFont(13)
        status.frame = CGRect(x: 46, y: 21, width: 330, height: 24)
        let spinner = NSProgressIndicator(frame: CGRect(x: 18, y: 24, width: 17, height: 17))
        spinner.style = .spinning
        spinner.controlSize = .small
        spinner.startAnimation(nil)
        let panel = NSPanel(contentRect: CGRect(x: 0, y: 0, width: 400, height: 64),
                            styleMask: [.titled], backing: .buffered, defer: false)
        panel.title = "检查更新"
        panel.isFloatingPanel = true
        panel.contentView?.addSubview(spinner)
        panel.contentView?.addSubview(status)
        panel.center()
        panel.orderFront(nil)

        Task { @MainActor in
            defer {
                updating = false
                panel.orderOut(nil)
            }
            do {
                let result = try await MacUpdater.check()
                panel.orderOut(nil)
                guard let asset = result.asset else {
                    let installed = result.installed
                    let latest = result.latest
                    if installed == latest {
                        updateNotice("当前已是最新版", detail: "fyto \(installed)")
                    } else {
                        updateNotice("暂无可安装的新版",
                                     detail: "已安装 \(installed)，GitHub 最新正式版为 \(latest)。")
                    }
                    return
                }

                let confirm = NSAlert()
                confirm.messageText = "发现 fyto \(asset.version)"
                confirm.informativeText = "安装后会重新打开 fyto，并恢复当前图片。"
                confirm.addButton(withTitle: "安装并重新打开")
                confirm.addButton(withTitle: "取消")
                guard confirm.runModal() == .alertFirstButtonReturn else { return }

                status.stringValue = "正在下载、校验并安装…"
                panel.orderFront(nil)
                try await MacUpdater.install(asset, replacing: result.installed, reopen: photo?.url)
                panel.orderOut(nil)
                updating = false
                NSApp.terminate(nil)
            } catch {
                panel.orderOut(nil)
                updateNotice("更新未完成", detail: error.localizedDescription)
            }
        }
    }

    private func updateNotice(_ title: String, detail: String) {
        let alert = NSAlert()
        alert.messageText = title
        alert.informativeText = detail
        alert.addButton(withTitle: "确定")
        alert.runModal()
    }

    func menu() -> NSMenu {
        let menu = NSMenu(title: "fyto")
        menu.font = viewerFont(13)
        func item(_ title: String, _ action: Selector, key: String = "", modifiers: NSEvent.ModifierFlags = []) -> NSMenuItem {
            let item = NSMenuItem(title: title, action: action, keyEquivalent: key)
            item.target = self
            item.keyEquivalentModifierMask = modifiers
            menu.addItem(item)
            return item
        }
        _ = item(standard ? "切换极简模式" : "切换标准模式", #selector(toggleMode), key: "1")
        let pin = item("钉住窗口", #selector(togglePin))
        pin.state = pinned ? .on : .off
        _ = item(playing ? "暂停播放" : "播放图片", #selector(togglePlayback), key: "p")
        menu.addItem(.separator())
        _ = item("打开图片…", #selector(openDialog), key: "o", modifiers: .command)
        _ = item("在 Finder 中显示", #selector(showInFinder))
        let metadata = item("EXIF 信息", #selector(toggleMetadata), key: "a")
        metadata.state = showsMetadata ? .on : .off
        _ = item("顺时针旋转", #selector(clockwise), key: String(UnicodeScalar(NSRightArrowFunctionKey)!), modifiers: .control)
        _ = item("逆时针旋转", #selector(counterclockwise), key: String(UnicodeScalar(NSLeftArrowFunctionKey)!), modifiers: .control)
        _ = item(expanded ? "恢复窗口" : "最大化", #selector(toggleMaximized), key: "f")
        _ = item("隐藏窗口", #selector(hideWindow), key: "m", modifiers: .command)
        _ = item("关闭", #selector(closeWindow), key: "w", modifiers: .command)
        let help = NSMenuItem(title: "帮助", action: nil, keyEquivalent: "")
        let helpMenu = NSMenu(title: "帮助")
        helpMenu.font = viewerFont(13)
        helpMenu.autoenablesItems = false
        let update = NSMenuItem(title: updating ? "正在更新…" : "检查更新…",
                                action: #selector(checkForUpdates), keyEquivalent: "")
        update.target = self
        update.isEnabled = !updating
        helpMenu.addItem(update)
        help.submenu = helpMenu
        menu.addItem(help)
        return menu
    }
}

@main
enum FytoMain {
    @MainActor static func main() {
        let application = NSApplication.shared
        application.setActivationPolicy(.accessory)
        let arguments = Array(CommandLine.arguments.dropFirst())
        if arguments.first == "--verify-update-archive", arguments.count == 2 {
            do { try MacUpdater.verifyArchive(URL(fileURLWithPath: arguments[1])) }
            catch {
                FileHandle.standardError.write(Data((error.localizedDescription + "\n").utf8))
                exit(1)
            }
            return
        }

        let viewer = Fyto()
        if arguments.contains("--verify") {
            do { try verify(application, viewer) }
            catch {
                FileHandle.standardError.write(Data((error.localizedDescription + "\n").utf8))
                exit(1)
            }
            return
        }
        application.delegate = viewer
        withExtendedLifetime(viewer) { application.run() }
    }

    @MainActor private static func verify(_ application: NSApplication, _ viewer: Fyto) throws {
        precondition(Bundle.main.object(forInfoDictionaryKey: "LSUIElement") as? Bool == true)
        precondition(application.activationPolicy() == .accessory)
        let area = CGSize(width: 1100, height: 760)
        let portrait = imageFit(CGSize(width: 2000, height: 4000), in: area, longEdge: 720)
        let landscape = imageFit(CGSize(width: 4000, height: 2000), in: area, longEdge: 720)
        precondition(portrait.height == landscape.width && portrait.width == landscape.height)
        let previous = viewer.pinned
        viewer.togglePin()
        precondition(viewer.window.level == (viewer.pinned ? .floating : .normal))
        precondition(viewer.pinned != previous)
        viewer.togglePin()
        precondition(viewer.pinned == previous)
        PhotoMetadata.verify()
        ViewerCanvas.verifyPinAlignment()
        viewer.verifyModeFitting()
        try MacUpdater.verifyFeed()
        print("macOS accessory policy, embedded font, long-edge sizing and topmost state verified")
    }
}