import AppKit
import QuartzCore

private final class PhotoLayer: NSView {
    override var isFlipped: Bool { true }
    override func hitTest(_ point: NSPoint) -> NSView? { nil }
}

private final class GlyphButton: NSButton {
    enum Glyph { case pin, minimize, maximize, close, playback }

    let glyph: Glyph
    var selected = false { didSet { needsDisplay = true } }
    var shadowed = false { didSet { needsDisplay = true } }

    init(_ glyph: Glyph, label: String) {
        self.glyph = glyph
        super.init(frame: .zero)
        isBordered = false
        setButtonType(.momentaryChange)
        setAccessibilityLabel(label)
        toolTip = label
    }

    required init?(coder: NSCoder) { return nil }
    override var isFlipped: Bool { true }

    override func draw(_ dirtyRect: NSRect) {
        if cell?.isHighlighted == true {
            NSColor(white: 1, alpha: 0.14).setFill()
            NSBezierPath(roundedRect: bounds.insetBy(dx: 4, dy: 4), xRadius: 4, yRadius: 4).fill()
        }
        let x = bounds.midX, y = bounds.midY
        let path = NSBezierPath()
        path.lineWidth = 1.35
        path.lineJoinStyle = .round
        path.lineCapStyle = .round

        func line(_ x1: CGFloat, _ y1: CGFloat, _ x2: CGFloat, _ y2: CGFloat) {
            path.move(to: CGPoint(x: x + x1, y: y + y1))
            path.line(to: CGPoint(x: x + x2, y: y + y2))
        }

        NSGraphicsContext.saveGraphicsState()
        if shadowed {
            let shadow = NSShadow()
            shadow.shadowColor = NSColor(white: 0, alpha: 0.42)
            shadow.shadowBlurRadius = 2
            shadow.shadowOffset = NSSize(width: 0, height: 0.5)
            shadow.set()
        }
        NSColor.white.set()
        switch glyph {
        case .pin:
            let points: [(CGFloat, CGFloat)] = [
                (5, 2), (15, 2), (15, 5), (13, 5), (13, 10), (17, 13),
                (17, 15), (11, 15), (11, 20), (10, 21), (9, 20), (9, 15),
                (3, 15), (3, 13), (7, 10), (7, 5), (5, 5)
            ]
            let scale = min(14, min(bounds.width, bounds.height) - 2) / 22
            for (index, point) in points.enumerated() {
                let p = CGPoint(x: x + (point.0 - 10) * scale, y: y + (point.1 - 11) * scale)
                if index == 0 { path.move(to: p) } else { path.line(to: p) }
            }
            path.close()
            if selected { path.fill() } else { path.stroke() }
        case .minimize:
            line(-6, 2, 6, 2)
            path.stroke()
        case .maximize:
            path.appendRect(NSRect(x: x - 5.5, y: y - 5.5, width: 11, height: 11))
            path.stroke()
        case .close:
            line(-5, -5, 5, 5)
            line(5, -5, -5, 5)
            path.stroke()
        case .playback:
            if selected {
                path.appendRect(NSRect(x: x - 7, y: y - 9, width: 5, height: 18))
                path.appendRect(NSRect(x: x + 2, y: y - 9, width: 5, height: 18))
            } else {
                path.move(to: CGPoint(x: x - 6, y: y - 9))
                path.line(to: CGPoint(x: x + 8, y: y))
                path.line(to: CGPoint(x: x - 6, y: y + 9))
                path.close()
            }
            path.fill()
        }
        NSGraphicsContext.restoreGraphicsState()
    }
}

@MainActor
final class ViewerCanvas: NSView {
    weak var viewer: Fyto?
    private let photoLayer = PhotoLayer()
    private let metadataPanel = MetadataPanel()
    private let image = NSImageView()
    private let filename = NSTextField(labelWithString: "")
    private let resolution = NSTextField(labelWithString: "")
    private let open = NSButton(title: "打开图片", target: nil, action: nil)
    private let menuButton = NSButton(title: "菜单", target: nil, action: nil)
    private let pin = GlyphButton(.pin, label: "钉住窗口")
    private let minimize = GlyphButton(.minimize, label: "隐藏窗口")
    private let maximize = GlyphButton(.maximize, label: "最大化或恢复窗口")
    private let close = GlyphButton(.close, label: "关闭")
    private let playback = GlyphButton(.playback, label: "播放图片")
    private var pixels = CGSize.zero
    private var viewport = CGRect.zero
    private var hover = false
    private var tracking: NSTrackingArea?
    private var dragStart: NSEvent?
    private var pan = CGPoint.zero
    private(set) var photoFrame = CGRect.zero
    private(set) var zoom: CGFloat = 1

    override var isFlipped: Bool { true }
    override var isOpaque: Bool { true }

    init() {
        super.init(frame: .zero)
        photoLayer.wantsLayer = true
        photoLayer.layer?.masksToBounds = true
        image.imageScaling = .scaleAxesIndependently
        image.animates = true
        photoLayer.addSubview(image)
        addSubview(photoLayer)
        addSubview(metadataPanel)
        metadataPanel.onClose = { [weak self] in self?.viewer?.toggleMetadata() }
        for label in [filename, resolution] {
            label.font = viewerFont(13)
            label.textColor = .white
            label.lineBreakMode = .byTruncatingMiddle
            label.maximumNumberOfLines = 1
            label.wantsLayer = true
            label.layer?.shadowColor = NSColor.black.cgColor
            label.layer?.shadowOffset = .zero
            label.layer?.shadowRadius = 3
            label.layer?.shadowOpacity = 0.44
            label.alphaValue = 0
            addSubview(label)
        }
        filename.alignment = .center
        resolution.alignment = .right
        resolution.font = viewerFont(12)
        for button in [open, menuButton] {
            button.font = viewerFont(13)
            button.isBordered = false
            button.attributedTitle = NSAttributedString(string: button.title, attributes: [
                .font: viewerFont(13), .foregroundColor: NSColor.white
            ])
        }
        open.target = self
        open.action = #selector(openFile)
        menuButton.target = self
        menuButton.action = #selector(showMenu)
        pin.target = self
        pin.action = #selector(pinWindow)
        minimize.target = self
        minimize.action = #selector(hideWindow)
        maximize.target = self
        maximize.action = #selector(maximizeWindow)
        close.target = self
        close.action = #selector(closeWindow)
        playback.target = self
        playback.action = #selector(play)
        playback.shadowed = true
        for button in [open, menuButton, pin, minimize, maximize, close, playback] { addSubview(button) }
        registerForDraggedTypes([.fileURL])
    }

    required init?(coder: NSCoder) { return nil }

    override func draw(_ dirtyRect: NSRect) {
        NSColor(white: 0.11, alpha: 1).setFill()
        NSBezierPath.fill(dirtyRect)
    }

    override func layout() {
        super.layout()
        let standard = viewer?.standard == true
        let chrome = standard ? titleHeight : 0
        let sidebar = viewer?.metadataWidth(in: bounds.width) ?? 0
        viewport = CGRect(x: 0, y: chrome, width: max(0, bounds.width - sidebar), height: max(0, bounds.height - chrome))
        metadataPanel.frame = CGRect(x: viewport.maxX, y: chrome, width: sidebar, height: viewport.height)
        let fit = imageFit(pixels, in: viewport.size)
        let size = CGSize(width: fit.width * zoom, height: fit.height * zoom)
        let maxX = max(0, (size.width - viewport.width) / 2)
        let maxY = max(0, (size.height - viewport.height) / 2)
        pan.x = min(maxX, max(-maxX, pan.x))
        pan.y = min(maxY, max(-maxY, pan.y))
        photoFrame = CGRect(x: viewport.midX - size.width / 2 + pan.x, y: viewport.midY - size.height / 2 + pan.y,
                            width: size.width, height: size.height)
        photoLayer.frame = viewport
        image.frame = photoFrame.offsetBy(dx: -viewport.minX, dy: -viewport.minY)
        let visible = viewport.intersection(photoFrame)
        let slot = min(44, bounds.width / 4)
        let margin = min(max(72, 4 * slot + 12), bounds.width / 2)
        filename.frame = CGRect(x: margin, y: 9, width: max(0, bounds.width - 2 * margin), height: 22)
        let pinHeight: CGFloat = 26
        let pinY = filename.frame.midY - pinHeight / 2
        if standard {
            pin.frame = CGRect(x: bounds.width - 4 * slot, y: pinY, width: slot, height: pinHeight)
        } else {
            let width = min(34, visible.width - 24)
            pin.frame = !visible.isNull && width >= 16
                ? CGRect(x: visible.maxX - 12 - width, y: pinY, width: width, height: pinHeight) : .zero
        }
        for (offset, button) in [minimize, maximize, close].enumerated() {
            button.frame = CGRect(x: bounds.width - CGFloat(3 - offset) * slot, y: 0, width: slot, height: titleHeight)
        }
        menuButton.frame = CGRect(x: 8, y: 0, width: min(64, max(0, bounds.width - 4 * slot - 8)), height: titleHeight)
        open.frame = CGRect(x: bounds.midX - 68, y: viewport.midY - 18, width: 136, height: 36)
        if visible.isNull {
            playback.frame = .zero
            resolution.frame = .zero
        } else {
            let playSize = max(0, min(40, visible.width - 24, visible.height - 24))
            playback.frame = CGRect(x: visible.midX - playSize / 2, y: visible.maxY - 12 - playSize,
                                    width: playSize, height: playSize)
            let left = playback.frame.maxX + 8
            resolution.frame = CGRect(x: left, y: visible.maxY - 32,
                                      width: max(0, visible.maxX - 12 - left), height: 20)
        }
    }

    func setPhoto(_ photo: NSImage, pixels: CGSize, name: String, resetZoom: Bool) {
        image.image = photo
        self.pixels = pixels
        filename.stringValue = name
        resolution.stringValue = "\(Int(pixels.width)) × \(Int(pixels.height))"
        if resetZoom { self.resetZoom() }
        refreshControls()
    }

    func showMetadata(_ value: PhotoMetadata) { metadataPanel.show(value) }

    func refreshControls(animated: Bool = false) {
        let standard = viewer?.standard == true
        let hasPhoto = image.image != nil
        metadataPanel.isHidden = viewer?.showsMetadata != true
        for button in [menuButton, minimize, maximize, close] { button.isHidden = !standard }
        open.isHidden = hasPhoto
        pin.isHidden = !standard && (!hover || !hasPhoto)
        pin.selected = viewer?.pinned == true
        pin.shadowed = !standard
        pin.setAccessibilityLabel(pin.selected ? "取消窗口置顶" : "钉住窗口")
        pin.toolTip = pin.selected ? "取消窗口置顶" : "钉住窗口"
        playback.isHidden = !hover || !hasPhoto
        playback.selected = viewer?.playing == true
        playback.setAccessibilityLabel(playback.selected ? "暂停播放" : "播放图片")
        filename.layer?.shadowOpacity = standard ? 0 : 0.44
        if !hasPhoto { filename.stringValue = "fyto" }

        let nameAlpha: CGFloat = standard || (hover && hasPhoto) ? 1 : 0
        let resolutionAlpha: CGFloat = hover && hasPhoto ? 1 : 0
        if animated {
            NSAnimationContext.runAnimationGroup { context in
                context.duration = hover ? 0.14 : 0.18
                filename.animator().alphaValue = nameAlpha
                resolution.animator().alphaValue = resolutionAlpha
            }
        } else {
            filename.alphaValue = nameAlpha
            resolution.alphaValue = resolutionAlpha
        }
        needsLayout = true
    }

    func resetZoom() {
        zoom = 1
        pan = .zero
        needsLayout = true
    }

    func magnify(by scale: CGFloat) {
        zoom = min(12, max(1, zoom * scale))
        if zoom == 1 { pan = .zero }
        needsLayout = true
    }

    override func updateTrackingAreas() {
        if let tracking { removeTrackingArea(tracking) }
        let area = NSTrackingArea(rect: .zero, options: [.activeAlways, .inVisibleRect, .mouseMoved,
                                                        .mouseEnteredAndExited], owner: self, userInfo: nil)
        tracking = area
        addTrackingArea(area)
        super.updateTrackingAreas()
    }

    private func setHover(_ value: Bool) {
        guard hover != value else { return }
        hover = value
        refreshControls(animated: true)
    }

    private func containsHover(_ point: CGPoint) -> Bool {
        let visible = viewport.intersection(photoFrame)
        if visible.contains(point) { return true }
        guard hover, viewer?.standard != true, !visible.isNull, !pin.frame.isEmpty else { return false }
        var reach = pin.frame
        reach.size.height = max(reach.maxY, visible.minY) - reach.minY
        return reach.contains(point)
    }

    override func mouseMoved(with event: NSEvent) {
        setHover(containsHover(convert(event.locationInWindow, from: nil)))
    }

    static func verifyPinAlignment() {
        let canvas = ViewerCanvas()
        let dimensions = CGSize(width: 1600, height: 400)
        canvas.frame = CGRect(x: 0, y: 0, width: 960, height: 640)
        canvas.setPhoto(NSImage(size: dimensions), pixels: dimensions, name: "Pin layout", resetZoom: true)
        canvas.layout()
        precondition(canvas.pin.frame.midY == canvas.filename.frame.midY)
        precondition(canvas.photoFrame.minY > canvas.pin.frame.maxY)
        let point = CGPoint(x: canvas.pin.frame.midX, y: canvas.pin.frame.midY)
        precondition(!canvas.containsHover(point))
        canvas.hover = true
        precondition(canvas.containsHover(point))
        print("macOS pin alignment and letterbox hover verified")
    }

    override func mouseEntered(with event: NSEvent) { mouseMoved(with: event) }
    override func mouseExited(with event: NSEvent) { setHover(false) }
    override func scrollWheel(with event: NSEvent) { viewer?.scroll(event) }
    override func magnify(with event: NSEvent) { magnify(by: 1 + event.magnification) }

    override func mouseDown(with event: NSEvent) {
        if event.clickCount == 2 {
            viewer?.toggleMaximized()
        } else {
            dragStart = event
        }
    }

    override func mouseDragged(with event: NSEvent) {
        guard let start = dragStart else { return }
        let point = convert(start.locationInWindow, from: nil)
        if zoom > 1 && point.y > titleHeight {
            pan.x += event.deltaX
            pan.y += event.deltaY
            needsLayout = true
        } else {
            let distance = hypot(event.locationInWindow.x - start.locationInWindow.x,
                                 event.locationInWindow.y - start.locationInWindow.y)
            guard distance >= 3 else { return }
            dragStart = nil
            viewer?.dragWindow(with: start)
        }
    }

    override func mouseUp(with event: NSEvent) { dragStart = nil }

    override func rightMouseDown(with event: NSEvent) {
        if let menu = viewer?.menu() { NSMenu.popUpContextMenu(menu, with: event, for: self) }
    }

    private func droppedFiles(_ sender: NSDraggingInfo) -> [URL] {
        let options: [NSPasteboard.ReadingOptionKey: Any] = [.urlReadingFileURLsOnly: true]
        let objects = sender.draggingPasteboard.readObjects(forClasses: [NSURL.self], options: options) ?? []
        return objects.compactMap { $0 as? URL }.filter { supportedExtensions.contains($0.pathExtension.lowercased()) }
    }

    override func draggingEntered(_ sender: NSDraggingInfo) -> NSDragOperation {
        droppedFiles(sender).isEmpty ? [] : .copy
    }

    override func performDragOperation(_ sender: NSDraggingInfo) -> Bool {
        let files = droppedFiles(sender)
        guard !files.isEmpty else { return false }
        viewer?.open(files)
        return true
    }

    @objc private func openFile() { viewer?.openDialog() }
    @objc private func pinWindow() { viewer?.togglePin() }
    @objc private func hideWindow() { setHover(false); viewer?.hideWindow() }
    @objc private func maximizeWindow() { viewer?.toggleMaximized() }
    @objc private func closeWindow() { viewer?.closeWindow() }
    @objc private func play() { viewer?.togglePlayback() }
    @objc private func showMenu() {
        viewer?.menu().popUp(positioning: nil, at: CGPoint(x: menuButton.frame.minX, y: menuButton.frame.maxY), in: self)
    }
}