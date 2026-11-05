import AppKit
import CoreText
import ImageIO
import UniformTypeIdentifiers

let titleHeight: CGFloat = 36
let supportedExtensions = Set(["jpg", "jpeg", "png", "gif", "bmp", "tif", "tiff",
                               "webp", "heic", "heif", "ico", "avif"])

func imageFit(_ size: CGSize, in limit: CGSize, longEdge: CGFloat = .greatestFiniteMagnitude) -> CGSize {
    guard size.width > 0, size.height > 0, limit.width > 0, limit.height > 0 else { return .zero }
    let scale = min(limit.width / size.width, limit.height / size.height,
                    longEdge / max(size.width, size.height))
    return CGSize(width: max(1, floor(size.width * scale)), height: max(1, floor(size.height * scale)))
}

func registerViewerFont() {
    if let url = Bundle.main.url(forResource: "SourceHanSansCN-Regular", withExtension: "otf") {
        CTFontManagerRegisterFontsForURL(url as CFURL, .process, nil)
    }
}

func viewerFont(_ size: CGFloat) -> NSFont {
    NSFont(name: "SourceHanSansCN-Regular", size: size) ?? NSFont.systemFont(ofSize: size)
}

struct Photo {
    let url: URL
    let image: NSImage
    let pixels: CGSize

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
        self.pixels = CGSize(width: swapped ? height.doubleValue : width.doubleValue,
                             height: swapped ? width.doubleValue : height.doubleValue)
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
    private var files: [URL] = []
    private var index = 0
    private var turns = 0
    private var slideshow: Timer?
    private var manualLongEdge: CGFloat?
    private var expanded = false
    private var restoreFrame: NSRect?
    private var scrollAmount: CGFloat = 0

    var playing: Bool { slideshow != nil }
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

    @objc func toggleMode() {
        let previous = window.frame
        standard.toggle()
        UserDefaults.standard.set(standard, forKey: "standardMode")
        canvas.refreshControls()
        guard let photo, let screen, canvas.zoom == 1 else { return }
        let work = screen.visibleFrame
        let fillsHeight = abs(previous.height - work.height) <= 2
        guard expanded || fillsHeight || abs(previous.width - work.width) <= 2 else { return }
        let maximum = NSSize(width: expanded || fillsHeight ? work.width : min(work.width, previous.width),
                             height: min(work.height, expanded ? work.height : previous.height))
        let chrome = standard ? titleHeight : 0
        let fit = imageFit(photo.size(turned: turns),
                           in: CGSize(width: maximum.width, height: maximum.height - chrome))
        guard fit.width > 0, fit.height > 0 else { return }
        let size = CGSize(width: max(64, fit.width), height: max(64, fit.height + chrome))
        window.setFrame(clamp(NSRect(x: previous.midX - size.width / 2, y: previous.midY - size.height / 2,
                                     width: size.width, height: size.height), to: work), display: true)
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
    @objc func closeWindow() { window.close() }

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
            let longEdge = min(manualLongEdge ?? 1980 / screen.backingScaleFactor,
                               work.width, work.height - chrome)
            let fitted = imageFit(photo.size(turned: turns),
                                  in: CGSize(width: work.width, height: work.height - chrome), longEdge: longEdge)
            size = CGSize(width: max(64, fitted.width), height: max(64, fitted.height + chrome))
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
        case "f": if !event.isARepeat { toggleMaximized() }; return true
        case "0": canvas.resetZoom(); return true
        default: return false
        }
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
        _ = item("顺时针旋转", #selector(clockwise), key: String(UnicodeScalar(NSRightArrowFunctionKey)!), modifiers: .control)
        _ = item("逆时针旋转", #selector(counterclockwise), key: String(UnicodeScalar(NSLeftArrowFunctionKey)!), modifiers: .control)
        _ = item(expanded ? "恢复窗口" : "最大化", #selector(toggleMaximized), key: "f")
        _ = item("隐藏窗口", #selector(hideWindow), key: "m", modifiers: .command)
        _ = item("关闭", #selector(closeWindow), key: "w", modifiers: .command)
        return menu
    }
}

@main
enum FytoMain {
    @MainActor static func main() {
        registerViewerFont()
        let application = NSApplication.shared
        application.setActivationPolicy(.accessory)
        let viewer = Fyto()
        if CommandLine.arguments.dropFirst().contains("--verify") {
            verify(application, viewer)
            return
        }
        application.delegate = viewer
        withExtendedLifetime(viewer) { application.run() }
    }

    @MainActor private static func verify(_ application: NSApplication, _ viewer: Fyto) {
        precondition(Bundle.main.object(forInfoDictionaryKey: "LSUIElement") as? Bool == true)
        precondition(application.activationPolicy() == .accessory)
        precondition(viewerFont(13).fontName == "SourceHanSansCN-Regular")
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
        print("macOS accessory policy, embedded font, long-edge sizing and topmost state verified")
    }
}