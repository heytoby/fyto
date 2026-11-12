from pathlib import Path
import sys

path = Path(sys.argv[1]) / 'voidImageViewer/src/viv.c'
s = path.read_text(encoding='utf-8')
def swap(a, b):
    global s
    assert s.count(a) == 1, a
    s = s.replace(a, b)

header = r'''
#define FYTO_TEST_PAINT (WM_APP + 0x731)
static unsigned _fyto_test_id(const wchar_t *path)
{
    const wchar_t *name = string_get_filename_part(path);
    unsigned n = 0;
    while (*name >= L'0' && *name <= L'9') n = n * 10 + *name++ - L'0';
    return n;
}
static void _fyto_test_event(DWORD event, DWORD value)
{
    wchar_t path[1024];
    DWORD length = GetEnvironmentVariableW(L"FYTO_TEST_TRACE", path, ARRAYSIZE(path));
    HANDLE file;
    DWORD wrote;
    struct { LONGLONG qpc; DWORD event, value; } record;
    LARGE_INTEGER now;
    if (!length || length >= ARRAYSIZE(path)) return;
    QueryPerformanceCounter(&now);
    record.qpc = now.QuadPart;
    record.event = event;
    record.value = value;
    file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_WRITE | FILE_SHARE_READ,
        NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);
    if (file == INVALID_HANDLE_VALUE) return;
    WriteFile(file, &record, sizeof(record), &wrote, NULL);
    CloseHandle(file);
}
static void _fyto_test_delay(const wchar_t *key, DWORD event, DWORD milliseconds)
{
    wchar_t enabled[2];
    if (GetEnvironmentVariableW(key, enabled, ARRAYSIZE(enabled)))
    {
        _fyto_test_event(event, 0);
        Sleep(milliseconds);
    }
}
'''
swap('static int _viv_main(int nCmdShow);', header + 'static int _viv_main(int nCmdShow);')
s = s.replace('scan->changes = FindFirstChangeNotificationW(', '_fyto_test_delay(L"FYTO_TEST_SLOW_DIRECTORY", 90, 1600);\n    scan->changes = FindFirstChangeNotificationW(', 1)
start = s.index('static DWORD WINAPI _viv_load_image_thread_proc(void *param)\n{')
i = s.index('\ttickstart = GetTickCount();', start)
s = s[:i] + '''    if (_fyto_test_id(job->filename) == 2)
        _fyto_test_delay(L"FYTO_TEST_SLOW_IMAGE", 91, 6000);
''' + s[i:]

first = s.index('static void _viv_start_first_frame(void)\n{')
i = s.index('\n\tUpdateWindow(_viv_hwnd);\n', first)
s = s[:i] + s[i:].replace('\n\tUpdateWindow(_viv_hwnd);\n', '''
\tUpdateWindow(_viv_hwnd);
    SendMessageW(_viv_hwnd, FYTO_TEST_PAINT, 0, 0);
    _fyto_test_event(_viv_preview.hbitmap ? 1 : 2, _fyto_test_id(_viv_frame_fd->cFileName));
''', 1)
swap('                                        UpdateWindow(_viv_hwnd);\n                                    }',
'''                                        UpdateWindow(_viv_hwnd);
                                        SendMessageW(_viv_hwnd, FYTO_TEST_PAINT, 0, 0);
                                        _fyto_test_event(2, _fyto_test_id(_viv_frame_fd->cFileName));
                                    }''')

start = s.index('\t\tcase WM_PAINT:\n\t\t{')
end = s.index('\t\tcase WM_ERASEBKGND:', start)
paint = s[start:end]
def in_paint(old, new):
    global paint
    assert paint.count(old) == 1, old
    paint = paint.replace(old, new)
in_paint('\t\tcase WM_PAINT:', '\t\tcase FYTO_TEST_PAINT:\n\t\tcase WM_PAINT:')
in_paint('PAINTSTRUCT ps;', '''PAINTSTRUCT ps = {0};
            HBITMAP test_bitmap = NULL;
            HGDIOBJ test_previous = NULL;
            BOOL offscreen = msg == FYTO_TEST_PAINT;''')
in_paint('if (BeginPaint(hwnd,&ps))','''if (offscreen)
            {
                GetClientRect(hwnd, &ps.rcPaint);
                ps.hdc = CreateCompatibleDC(NULL);
                test_bitmap = fyto_image_dib(ps.rcPaint.right, ps.rcPaint.bottom, 32, NULL);
                if (ps.hdc && test_bitmap) test_previous = SelectObject(ps.hdc, test_bitmap);
            }
            if ((offscreen && test_previous) || (!offscreen && BeginPaint(hwnd,&ps)))''')
in_paint('GetRandomRgn(ps.hdc,update_hrgn,SYSRGN);','''if (offscreen)
                    SetRectRgn(update_hrgn, 0, 0, ps.rcPaint.right, ps.rcPaint.bottom);
                else GetRandomRgn(ps.hdc,update_hrgn,SYSRGN);''')
in_paint('if (os_is_nt)', 'if (os_is_nt && !offscreen)')
in_paint('EndPaint(hwnd,&ps);','if (offscreen) GdiFlush();\n                else EndPaint(hwnd,&ps);')
in_paint('\n\t\t\treturn 0;\n\t\t}', '''
            if (offscreen)
            {
                if (test_previous) SelectObject(ps.hdc, test_previous);
                if (test_bitmap) DeleteObject(test_bitmap);
                if (ps.hdc) DeleteDC(ps.hdc);
            }
\t\t\treturn 0;
\t\t}''')
s = s[:start] + paint + s[end:]
path.write_bytes(s.replace('\n','\r\n').encode('utf-8'))
