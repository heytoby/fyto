from pathlib import Path
from tempfile import TemporaryDirectory
import os
import shutil
import subprocess
import sys
import time

from PIL import Image

from window_test_helpers import label, u, wait, wins

root = Path(__file__).resolve().parents[1]
viewer = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else root / 'voidImageViewer/vs2019/x64/Release/fyto.exe'

def wheel(delta):
    return 0x020A, (delta & 0xFFFF) << 16

def key(code):
    return 0x0100, code

def side(button):
    return 0x020B, button << 16

def check_navigation(folder, settings, after_forward, after_back, forward, modified_times=None):
    folder.mkdir()
    shutil.copy2(viewer, folder / 'fyto.exe')
    for number, color in [(1, (80, 120, 150)), (2, (120, 160, 80)), (10, (170, 100, 30))]:
        image = folder / f'{number}.png'
        Image.new('RGB', (1200, 800), color).save(image)
        if modified_times is not None:
            stamp = modified_times[number]
            os.utime(image, ns=(stamp, stamp))
    (folder / 'fyto.ini').write_text(
        '[fyto]\nappdata=0\nmultiple_instances=1\nshow_caption=1\n' + settings,
        encoding='ascii',
    )
    (folder / 'associations.ini').write_text('[fyto]\nSelectedFormats=15\nSetupVersion=1\n', encoding='ascii')
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    process = subprocess.Popen(
        [str(folder / 'fyto.exe'), '/window', str(folder / '2.png')],
        cwd=folder,
        startupinfo=startup,
    )
    hwnd = None
    try:
        wait(lambda: any(name == 'fyto' for _, name in wins(process.pid)))
        hwnd = next(h for h, name in wins(process.pid) if name == 'fyto')
        wait(lambda: label(hwnd) == '2.png')
        assert u.PostMessageW(hwnd, *wheel(forward // 2), 0)
        time.sleep(0.08)
        assert label(hwnd) == '2.png'
        for event, expected in [
            (wheel(forward // 2), after_forward), (wheel(-forward), '2.png'),
            (side(1), after_back), (side(2), '2.png'),
            (key(37), after_back), (key(39), '2.png'),
        ]:
            assert u.PostMessageW(hwnd, *event, 0)
            wait(lambda: label(hwnd) == expected)
    finally:
        if hwnd:
            u.PostMessageW(hwnd, 0x0010, 0, 0)
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.terminate()
            process.wait(timeout=5)

test_directory = root / 'build' / 'order-tests'
test_directory.mkdir(parents=True, exist_ok=True)
with TemporaryDirectory(prefix='fyto-order-', dir=test_directory.resolve()) as temp:
    scratch = Path(temp)
    if '--same-time-only' not in sys.argv:
        check_navigation(scratch / 'new', '', '10.png', '1.png', -120)
        check_navigation(scratch / 'upgraded', 'defaults_version=4\nmouse_wheel_action=2\n', '10.png', '1.png', -120)
        check_navigation(scratch / 'descending', 'defaults_version=4\nmouse_wheel_action=2\nsort_ascending=0\n', '1.png', '10.png', -120)
        check_navigation(scratch / 'custom', 'defaults_version=5\nmouse_wheel_action=2\n', '10.png', '1.png', 120)

    same_time = 1_700_000_000_000_000_000
    saved_sort = 'defaults_version=4\nmouse_wheel_action=1\nsort=2\nsort_ascending=1\n'
    check_navigation(scratch / 'equal-time', saved_sort, '10.png', '1.png', -120,
                     {1: same_time, 2: same_time, 10: same_time})
    check_navigation(scratch / 'different-times', saved_sort, '1.png', '10.png', -120,
                     {1: same_time + 2_000_000_000, 2: same_time + 1_000_000_000, 10: same_time})

print('Wheel detents, side buttons, arrow keys, migrated settings, and chosen sort/direction passed')
