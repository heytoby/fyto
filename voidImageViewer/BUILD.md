# Build

Build the viewer with Visual Studio Build Tools v142 and the Windows SDK using
vs2019/voidImageViewer.vcxproj, Release x64, and the archive-root build-overrides.targets.
The native viewer uses the static CRT and Windows system DLLs.

Build the installer using WiX 6.0.2 plus WixToolset.UI.wixext 6.0.2 and .NET 8
with DOTNET_ROLL_FORWARD=Major. installer/build-installer.ps1 takes the absolute
WiX DLL, UI extension DLL, payload folder, and output directory. The payload must
contain voidImageViewer.exe, LICENSE, libwebp-COPYING, libwebp-PATENTS, README.txt.
The x64 native EXE embeds a Chinese Windows Installer package; no network is
required when running setup. Original project license and attribution are retained.

The installer preserves an existing INI, uses registered old installation paths,
and supports normal Windows uninstall. Its one-time defaults migration applies
black background and left image panning. Later user color changes persist.

# Validation on 2026-10-27

Native Windows viewer tests passed for portrait/landscape/square resize in both
preload modes, preserved maximized geometry, native caption hit testing, image
panning without window movement, centered toolbar, default migration, empty state,
PNG/JPEG/WebP/GIF reading, file error states, and Esc exit. Footer geometry passed
96 combinations of width, DPI, and visibility.

Installer EXE launch with /verify successfully extracted and opened the embedded
MSI. MSI language, file version, config preservation, shortcuts and upgrade
registration rules were checked. Full install/upgrade/uninstall execution could
not be validated: the execution environment's Windows Installer service returned
1601 before installation. Actual Windows shell Snap dragging was not automated.

Linux build-linux.py is provided as a recovery build path. The delivered program
and setup were built with MSVC on Windows.

# Validation on 2026-10-30
Version 1.0.0.17 preserves the startup size during automatic minimal-mode fitting. Six same-image reopen cycles, six alternating-orientation cycles and three manual-resize reopen cycles passed. Native window regression tests passed. Installer /verify passed; full installation was not executed.

# Validation on 2026-10-31
Version 1.0.0.18 recalculates the startup envelope using monitor work area and existing Auto Fit fractions. Eighteen alternating-orientation startups passed across three stale geometries, including mode toggles before exit. Four fullscreen key-1 toggles passed with unchanged window bounds and visible/hidden toolbar and title client inset. Native restore from maximized ignores legacy 64x64 geometry. Actual mouse drag automation was blocked by unavailable SetCursorPos access. Window regression suite passed. Setup /verify passed; full installation was not executed.

# Menu hover fix, version 1.0.0.19
Detached the duplicate native menu bar while retaining HMENU for custom dropdowns. Native nonclient hover is suppressed over custom labels; menu exit repaints the frame/client. Twelve mode toggles across windowed/fullscreen states passed with client bitmap comparison before/after hover and six successful dropdown open/cancel cycles.
