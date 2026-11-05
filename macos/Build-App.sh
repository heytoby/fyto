#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
header="$root/voidImageViewer/src/version.h"
read -r major minor revision build < <(awk '
  $2=="VERSION_MAJOR" { major=$3+0 }
  $2=="VERSION_MINOR" { minor=$3+0 }
  $2=="VERSION_REVISION" { revision=$3+0 }
  $2=="VERSION_BUILD" { build=$3+0 }
  END { print major, minor, revision, build }
' "$header")
version="$major.$minor.$revision.$build"
mkdir -p "$root/build" "$root/release-assets"
work="$(mktemp -d "$root/build/fyto-macos.XXXXXX")"
app="$work/fyto.app"
resources="$app/Contents/Resources"
mkdir -p "$app/Contents/MacOS" "$resources"

font="$resources/SourceHanSansCN-Regular.otf"
font_blob=5e6605995d5cbc432c4273f566a5f2b8c376c682
bundled_font="$root/voidImageViewer/res/SourceHanSansCN-Regular.otf"
if [[ -f "$bundled_font" ]] && [[ "$(git hash-object --no-filters -- "$bundled_font")" == "$font_blob" ]]; then
    cp "$bundled_font" "$font"
else
    curl --fail --silent --show-error --location --retry 3 \
        https://raw.githubusercontent.com/adobe-fonts/source-han-sans/6c709ca72d3d7c46ab42ebecc1a26e7d69595a37/SubsetOTF/CN/SourceHanSansCN-Regular.otf \
        --output "$font"
fi
[[ "$(git hash-object --no-filters -- "$font")" == "$font_blob" ]]
cp "$root/LICENSE" "$root/THIRD_PARTY_NOTICES.md" "$resources/"
cp "$root/voidImageViewer/res/SourceHanSans-OFL.txt" "$resources/"

# Preserve the existing logo's proportions when preparing the Finder icon.
iconset="$work/fyto.iconset"
mkdir "$iconset"
sips -Z 896 "$root/voidImageViewer/res/fyto-logo-original.png" --out "$work/logo.png" >/dev/null
sips --padToHeightWidth 1024 1024 "$work/logo.png" --out "$work/square.png" >/dev/null
for points in 16 32 128 256 512; do
    sips -z "$points" "$points" "$work/square.png" --out "$iconset/icon_${points}x${points}.png" >/dev/null
    pixels=$((points*2))
    sips -z "$pixels" "$pixels" "$work/square.png" --out "$iconset/icon_${points}x${points}@2x.png" >/dev/null
done
iconutil -c icns "$iconset" -o "$resources/fyto.icns"

sed -e "s/@SHORT_VERSION@/$major.$minor.$build/g" -e "s/@BUILD@/$build/g" -e "s/@VERSION@/$version/g" \
    "$root/macos/Info.plist" > "$app/Contents/Info.plist"
plutil -lint "$app/Contents/Info.plist"

for architecture in arm64 x86_64; do
    xcrun swiftc -swift-version 5 -O -target "$architecture-apple-macos12.0" \
        "$root/macos/Fyto.swift" "$root/macos/ViewerCanvas.swift" -o "$work/fyto-$architecture"
done
xcrun lipo -create "$work/fyto-arm64" "$work/fyto-x86_64" -output "$app/Contents/MacOS/fyto"
chmod +x "$app/Contents/MacOS/fyto"
xcrun lipo "$app/Contents/MacOS/fyto" -verify_arch arm64 x86_64
codesign --force --sign - --timestamp=none "$app"
codesign --verify --deep --strict "$app"
"$app/Contents/MacOS/fyto" --verify

archive="$root/release-assets/fyto-$version-macos-universal.zip"
ditto -c -k --sequesterRsrc --keepParent "$app" "$archive"
shasum -a 256 "$archive"
