import AppKit
import ImageIO

struct PhotoMetadata {
    typealias Row = (label: String, value: String)
    let summary: [Row]
    let details: [Row]

    init(properties: [CFString: Any], dimensions: CGSize) {
        let tiff = properties[kCGImagePropertyTIFFDictionary] as? [String: Any] ?? [:]
        let exif = properties[kCGImagePropertyExifDictionary] as? [String: Any] ?? [:]
        let auxiliary = properties[kCGImagePropertyExifAuxDictionary] as? [String: Any] ?? [:]
        var rows: [Row] = [("分辨率", "\(Int(dimensions.width)) × \(Int(dimensions.height))")]

        func add(_ label: String, _ value: String?) {
            if let value, !value.isEmpty { rows.append((label, value)) }
        }
        func number(_ key: String) -> Double? {
            guard let value = exif[key] as? NSNumber, value.doubleValue.isFinite else { return nil }
            return value.doubleValue
        }
        let make = Self.value(tiff["Make"])
        let model = Self.value(tiff["Model"])
        if let make, let model {
            add("相机", model.lowercased().hasPrefix(make.lowercased()) ? model : "\(make) \(model)")
        } else {
            add("相机", model ?? make)
        }
        add("镜头", Self.value(exif["LensModel"]) ?? Self.value(auxiliary["LensModel"]))
        if let mm = number("FocalLength"), mm > 0 { add("焦距", "\(Self.decimal(mm)) mm") }
        if let mm = number("FocalLenIn35mmFilm"), mm > 0 { add("等效焦距", "\(Self.decimal(mm)) mm") }
        if let aperture = number("FNumber"), aperture > 0 { add("光圈", "f/\(Self.decimal(aperture))") }
        if let exposure = number("ExposureTime"), exposure > 0 {
            if exposure < 0.5 && exposure >= 0.000001 {
                add("快门", "1/\(Int((1 / exposure).rounded())) s")
            } else {
                add("快门", "\(Self.decimal(exposure)) s")
            }
        }
        let iso = (exif["ISOSpeedRatings"] as? [NSNumber])?.first ?? exif["PhotographicSensitivity"] as? NSNumber
        add("感光度", iso.map { "ISO \($0.intValue)" })
        if let bias = number("ExposureBiasValue") { add("曝光补偿", "\(Self.decimal(bias)) EV") }
        let taken = Self.value(exif["DateTimeOriginal"]) ?? Self.value(tiff["DateTime"])
        if let taken {
            let offset = Self.value(exif["OffsetTimeOriginal"])
            add("拍摄时间", offset.map { "\(taken) \($0)" } ?? taken)
        }
        summary = rows

        var all: [Row] = []
        for (key, value) in properties.map({ ($0.key as String, $0.value) }).sorted(by: { $0.0 < $1.0 }) {
            Self.flatten(value, under: key, depth: 0, into: &all)
        }
        details = all
    }

    private static func decimal(_ number: Double) -> String {
        let formatter = NumberFormatter()
        formatter.maximumFractionDigits = 3
        formatter.minimumFractionDigits = 0
        return formatter.string(from: NSNumber(value: number)) ?? String(number)
    }

    private static func value(_ item: Any?) -> String? {
        switch item {
        case let text as String:
            return String(text.trimmingCharacters(in: .whitespacesAndNewlines).prefix(2000))
        case let number as NSNumber:
            return number.stringValue
        case let bytes as Data:
            return "\(bytes.count) 字节"
        case let values as [Any]:
            return values.prefix(32).compactMap { value($0) }.joined(separator: "，")
        default:
            return nil
        }
    }

    private static func flatten(_ item: Any, under key: String, depth: Int, into rows: inout [Row]) {
        guard depth <= 5, rows.count < 800 else { return }
        if let group = item as? [String: Any] {
            for name in group.keys.sorted() {
                if let value = group[name] { flatten(value, under: "\(key).\(name)", depth: depth + 1, into: &rows) }
            }
        } else if let value = value(item), !value.isEmpty {
            rows.append((key, value))
        }
    }

    static func verify() {
        let metadata = PhotoMetadata(properties: [
            kCGImagePropertyTIFFDictionary: ["Make": "SONY", "Model": "ILCE-7M4"],
            kCGImagePropertyExifDictionary: [
                "LensModel": "FE 24-70mm F2.8", "FNumber": 2.8, "ExposureTime": 1.0 / 250,
                "ISOSpeedRatings": [400], "FocalLength": 35, "DateTimeOriginal": "2026:11:05 12:00:00"
            ]
        ], dimensions: CGSize(width: 6000, height: 4000))
        precondition(metadata.summary.contains { $0.label == "相机" && $0.value == "SONY ILCE-7M4" })
        precondition(metadata.summary.contains { $0.value == "1/250 s" })
        precondition(metadata.summary.contains { $0.value == "ISO 400" })
        precondition(metadata.details.contains { $0.label.hasSuffix(".LensModel") && $0.value == "FE 24-70mm F2.8" })
        precondition(PhotoMetadata(properties: [:], dimensions: CGSize(width: 1, height: 1)).summary.count == 1)
        print("macOS EXIF summary, missing fields and detailed metadata verified")
    }
}

@MainActor
final class MetadataPanel: NSView {
    var onClose: (() -> Void)?
    private let heading = NSTextField(labelWithString: "EXIF 信息")
    private let close = NSButton(title: "×", target: nil, action: nil)
    private let mode = NSSegmentedControl(labels: ["摘要", "全部"], trackingMode: .selectOne, target: nil, action: nil)
    private let scroll = NSTextView.scrollableTextView()
    private var text: NSTextView { scroll.documentView as! NSTextView }
    private var metadata: PhotoMetadata?

    override var isFlipped: Bool { true }

    init() {
        super.init(frame: .zero)
        heading.font = viewerFont(14)
        heading.textColor = .white
        close.isBordered = false
        close.font = viewerFont(18)
        close.target = self
        close.action = #selector(closePanel)
        close.setAccessibilityLabel("关闭 EXIF 信息")
        mode.font = viewerFont(12)
        mode.selectedSegment = 0
        mode.target = self
        mode.action = #selector(refresh)
        scroll.drawsBackground = false
        scroll.borderType = .noBorder
        scroll.hasHorizontalScroller = false
        text.isEditable = false
        text.isSelectable = true
        text.drawsBackground = false
        text.textContainerInset = NSSize(width: 14, height: 8)
        for view in [heading, close, mode, scroll] { addSubview(view) }
    }

    required init?(coder: NSCoder) { return nil }

    override func draw(_ dirtyRect: NSRect) {
        NSColor(white: 0.14, alpha: 1).setFill()
        NSBezierPath.fill(dirtyRect)
        NSColor(white: 0.24, alpha: 1).setFill()
        NSBezierPath.fill(NSRect(x: 0, y: 0, width: 1, height: bounds.height))
    }

    override func layout() {
        super.layout()
        heading.frame = CGRect(x: 14, y: 11, width: max(0, bounds.width - 55), height: 25)
        close.frame = CGRect(x: max(0, bounds.width - 34), y: 7, width: 28, height: 30)
        mode.frame = CGRect(x: 14, y: 42, width: max(0, bounds.width - 28), height: 24)
        scroll.frame = CGRect(x: 1, y: 72, width: max(0, bounds.width - 1), height: max(0, bounds.height - 72))
    }

    func show(_ value: PhotoMetadata) {
        metadata = value
        refresh()
    }

    @objc private func refresh() {
        guard let metadata else { return }
        let rows = mode.selectedSegment == 0 ? metadata.summary : metadata.details
        let output = NSMutableAttributedString(string: "")
        let label: [NSAttributedString.Key: Any] = [.font: viewerFont(11), .foregroundColor: NSColor(white: 0.72, alpha: 1)]
        let value: [NSAttributedString.Key: Any] = [.font: viewerFont(13), .foregroundColor: NSColor.white]
        if rows.isEmpty {
            output.append(NSAttributedString(string: "此图片没有可读取的 EXIF 信息。", attributes: value))
        }
        for row in rows {
            output.append(NSAttributedString(string: row.label + "\n", attributes: label))
            output.append(NSAttributedString(string: row.value + "\n\n", attributes: value))
        }
        text.textStorage?.setAttributedString(output)
        text.scrollRangeToVisible(NSRange(location: 0, length: 0))
    }

    @objc private func closePanel() { onClose?() }
}