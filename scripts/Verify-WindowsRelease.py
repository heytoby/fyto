"""Verify and scan the exact Windows artifacts from a successful package run."""

import argparse
import ctypes
from ctypes import wintypes
from hashlib import sha256
from io import BytesIO
import json
from pathlib import Path
import re
from zipfile import ZipFile


def scan_installers(installers):
    amsi = ctypes.WinDLL("amsi.dll", winmode=0x800)
    amsi.AmsiInitialize.argtypes = [wintypes.LPCWSTR, ctypes.POINTER(ctypes.c_void_p)]
    amsi.AmsiInitialize.restype = wintypes.LONG
    amsi.AmsiScanBuffer.argtypes = [
        ctypes.c_void_p, ctypes.c_void_p, wintypes.ULONG, wintypes.LPCWSTR,
        ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint),
    ]
    amsi.AmsiScanBuffer.restype = wintypes.LONG
    amsi.AmsiUninitialize.argtypes = [ctypes.c_void_p]
    amsi.AmsiUninitialize.restype = None
    context = ctypes.c_void_p()
    status = amsi.AmsiInitialize("fyto release verification", ctypes.byref(context))
    if status != 0:
        raise SystemExit(f"AMSI initialization failed: 0x{status & 0xffffffff:08x}")
    try:
        results = {}
        for name, data in installers.items():
            buffer = ctypes.create_string_buffer(data)
            verdict = ctypes.c_uint()
            status = amsi.AmsiScanBuffer(
                context, buffer, len(data), name, None, ctypes.byref(verdict),
            )
            if status != 0:
                raise SystemExit(f"AMSI scan failed for {name}: 0x{status & 0xffffffff:08x}")
            results[name] = verdict.value
        return results
    finally:
        amsi.AmsiUninitialize(context)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path, help="fyto-Windows-x64 workflow artifact ZIP")
    parser.add_argument("version")
    parser.add_argument("--control", type=Path, help="workflow ZIP containing the detected 1.0.0.38 setup")
    args = parser.parse_args()
    if not re.fullmatch(r"1\.0\.0\.\d+", args.version):
        parser.error("expected a fyto version in 1.0.0.BUILD form")
    prefix = f"fyto-Setup-{args.version}"
    with ZipFile(args.archive) as archive:
        installers = {prefix + ext: archive.read(prefix + ext) for ext in (".msi", ".exe")}
        expected = {}
        for line in archive.read("SHA256SUMS.txt").decode("ascii").splitlines():
            digest, name = line.split("  ", 1)
            expected[name] = digest
        for name, data in installers.items():
            if sha256(data).hexdigest() != expected[name]:
                raise SystemExit(f"Checksum mismatch for {name}")
        with ZipFile(BytesIO(archive.read(prefix + ".zip"))) as download:
            if download.read(prefix + ".msi") != installers[prefix + ".msi"]:
                raise SystemExit("Windows ZIP contains a different MSI")
            if any(name.lower().endswith(".exe") for name in download.namelist()):
                raise SystemExit("Windows ZIP must contain the native MSI")

    if args.control:
        name = "fyto-Setup-1.0.0.38.exe"
        with ZipFile(args.control) as archive:
            control = archive.read(name)
        if sha256(control).hexdigest() != "f9a789e340392b9a7f8a0baceae21e95c98f7fd5027bfdffbc68ee4dc79c4614":
            raise SystemExit("Control is not the reported official EXE")
        result = scan_installers({name: control})[name]
        if result < 32768:
            raise SystemExit(f"AMSI did not reproduce the control detection (result {result})")
        print(f"Confirmed previous setup detection (AMSI result {result})")

    results = scan_installers(installers)
    for name, result in results.items():
        if result not in (0, 1):
            raise SystemExit(f"{name} did not pass AMSI scanning (result {result})")
    print(json.dumps({name: {"sha256": expected[name], "amsi_result": result}
                      for name, result in results.items()}))


if __name__ == "__main__":
    main()
