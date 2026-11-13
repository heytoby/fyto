from pathlib import Path
import sys

p=Path(sys.argv[1])/'voidImageViewer/src/viv.c'
s=p.read_text(encoding='utf-8')
def swap(a,b):
    global s
    assert s.count(a)==1,a
    s=s.replace(a,b)

header=r'''
#define FYTO_TEST_PAINT (WM_APP+0x731)
static unsigned fyto_test_id(const wchar_t *path)
{
    const wchar_t *name = string_get_filename_part(path);
    unsigned number = 0;
    while (*name >= L'0' && *name <= L'9') number = number*10 + *name++ - L'0';
    return number;
}
static void fyto_test_event(DWORD event, DWORD value)
{
    wchar_t path[1024];
    DWORD length = GetEnvironmentVariableW(L"FYTO_TEST_TRACE",path,ARRAYSIZE(path));
    DWORD written;
    HANDLE file;
    struct { LONGLONG qpc; DWORD event, value; } record;
    LARGE_INTEGER time;
    if (!length || length >= ARRAYSIZE(path)) return;
    QueryPerformanceCounter(&time);
    record.qpc = time.QuadPart;
    record.event = event;
    record.value = value;
    file = CreateFileW(path,FILE_APPEND_DATA,FILE_SHARE_WRITE|FILE_SHARE_READ,NULL,
        OPEN_ALWAYS,FILE_ATTRIBUTE_TEMPORARY,NULL);
    if (file == INVALID_HANDLE_VALUE) return;
    WriteFile(file,&record,sizeof(record),&written,NULL);
    CloseHandle(file);
}
static void fyto_test_delay(const wchar_t *key, DWORD event, DWORD duration)
{
    wchar_t enabled[2];
    if (GetEnvironmentVariableW(key,enabled,ARRAYSIZE(enabled)))
    {
        fyto_test_event(event,0);
        Sleep(duration);
    }
}
'''
swap('static int _viv_main(int nCmdShow);',header+'static int _viv_main(int nCmdShow);')
swap('        scan->changes = FindFirstChangeNotificationW(',
     '        fyto_test_delay(L"FYTO_TEST_SLOW_DIRECTORY",90,700);\n        scan->changes = FindFirstChangeNotificationW(')
swap('\tdebug_printf("%s %S...\\n",job->prefetch ? "PRELOAD" : "LOAD",job->filename);',
     '''    fyto_test_event(job->prefetch ? 40 : 30,fyto_test_id(job->filename));
    if (fyto_test_id(job->filename)==4)
        fyto_test_delay(L"FYTO_TEST_SLOW_IMAGE",91,650);''')
swap('            preview.hbitmap = fyto_jpeg_preview(&jpeg, edge, &preview.wide, &preview.high);',
     '            preview.hbitmap = fyto_jpeg_preview(&jpeg, edge, &preview.wide, &preview.high);\n            fyto_test_event(32,fyto_test_id(job->filename));')
swap('            frame->frame.hbitmap = fyto_jpeg_full(&jpeg, orientation, &frame->wide, &frame->high);',
     '            frame->frame.hbitmap = fyto_jpeg_full(&jpeg, orientation, &frame->wide, &frame->high);\n            fyto_test_event(33,fyto_test_id(job->filename));')
swap('\t_viv_reply_add(job, ret ? _VIV_REPLY_LOAD_IMAGE_COMPLETE : _VIV_REPLY_LOAD_IMAGE_FAILED,0,0);',
     '\tfyto_test_event(39,fyto_test_id(job->filename));\n\t_viv_reply_add(job, ret ? _VIV_REPLY_LOAD_IMAGE_COMPLETE : _VIV_REPLY_LOAD_IMAGE_FAILED,0,0);')
swap('\tUpdateWindow(_viv_hwnd);\n    title_deferred = was_deferred;',
     '''\tUpdateWindow(_viv_hwnd);
    SendMessageW(_viv_hwnd,FYTO_TEST_PAINT,0,0);
    fyto_test_event(_viv_preview.hbitmap?1:2,fyto_test_id(_viv_frame_fd->cFileName));
    title_deferred = was_deferred;''')
swap('                                        UpdateWindow(_viv_hwnd);\n                                    }',
     '''                                        UpdateWindow(_viv_hwnd);
                                        SendMessageW(_viv_hwnd,FYTO_TEST_PAINT,0,0);
                                        fyto_test_event(2,fyto_test_id(_viv_frame_fd->cFileName));
                                    }''')
swap('            fyto_test_event(32,fyto_test_id(job->filename));',
     '''            fyto_test_event(32,fyto_test_id(job->filename));
            fyto_test_event(preview.hbitmap ? 42 : 43,fyto_test_id(job->filename));''')
swap('                                if (selected || _viv_load_image_terminate || !_viv_preload_preview.hbitmap)',
     '''                                fyto_test_event(44, (selected ? 1 : 0) | (_viv_load_image_terminate ? 2 : 0) |
                                    (_viv_preload_preview.hbitmap ? 4 : 0) | (_viv_load_is_preload ? 8 : 0));
                                if (selected || _viv_load_image_terminate || !_viv_preload_preview.hbitmap)''')
swap('                        _viv_preview_t *preview = (_viv_preview_t *)(e + 1);',
     '''                        _viv_preview_t *preview = (_viv_preview_t *)(e + 1);
                        fyto_test_event(45, (_viv_load_is_preload ? 1 : 0) |
                            (_viv_should_activate_preload_on_load ? 2 : 0) | (_viv_load_image_terminate ? 4 : 0));''')
swap('    BOOL preserve_preload = FALSE;',
     '''    BOOL preserve_preload = FALSE;
    fyto_test_event(is_preload ? 46 : 47, fyto_test_id(fd->cFileName));
    fyto_test_event(48, (_viv_preload_preview.hbitmap ? 1 : 0) |
        (_viv_same_image(_viv_preload_fd,fd) ? 2 : 0) | (_viv_load_is_preload ? 4 : 0) |
        (_viv_active_load ? 8 : 0));''')
swap('    if (!fresh && _viv_directory_changed(directory))\n    {',
     '    if (!fresh && _viv_directory_changed(directory))\n    {\n        fyto_test_event(49,fyto_test_id(current->cFileName));')
swap('static void _viv_open_preload(void)\n{',
     'static void _viv_open_preload(void)\n{\n    fyto_test_event(50,fyto_test_id(_viv_preload_fd->cFileName));')

first=s.index('\t\tcase WM_PAINT:\n\t\t{')
last=s.index('\t\tcase WM_ERASEBKGND:',first)
paint=s[first:last]
def paint_swap(a,b):
    global paint
    assert paint.count(a)==1,a
    paint=paint.replace(a,b)
paint_swap('\t\tcase WM_PAINT:', '\t\tcase FYTO_TEST_PAINT:\n\t\tcase WM_PAINT:')
paint_swap('PAINTSTRUCT ps;', '''PAINTSTRUCT ps={0};
            HBITMAP test_bitmap=NULL;
            HGDIOBJ test_previous=NULL;
            BOOL offscreen=msg==FYTO_TEST_PAINT;''')
paint_swap('if (BeginPaint(hwnd,&ps))','''if (offscreen)
            {
                GetClientRect(hwnd,&ps.rcPaint);
                ps.hdc=CreateCompatibleDC(NULL);
                test_bitmap=fyto_image_dib(ps.rcPaint.right,ps.rcPaint.bottom,32,NULL);
                if (ps.hdc && test_bitmap) test_previous=SelectObject(ps.hdc,test_bitmap);
            }
            if ((offscreen && test_previous) || (!offscreen && BeginPaint(hwnd,&ps)))''')
paint_swap('GetRandomRgn(ps.hdc,update_hrgn,SYSRGN);','''if (offscreen)
                    SetRectRgn(update_hrgn,0,0,ps.rcPaint.right,ps.rcPaint.bottom);
                else GetRandomRgn(ps.hdc,update_hrgn,SYSRGN);''')
paint_swap('if (os_is_nt)','if (os_is_nt && !offscreen)')
paint_swap('EndPaint(hwnd,&ps);','if (offscreen) GdiFlush();\n                else EndPaint(hwnd,&ps);')
paint_swap('\n\t\t\treturn 0;\n\t\t}', '''
            if (offscreen)
            {
                if (test_previous) SelectObject(ps.hdc,test_previous);
                if (test_bitmap) DeleteObject(test_bitmap);
                if (ps.hdc) DeleteDC(ps.hdc);
            }
\t\t\treturn 0;
\t\t}''')
s=s[:first]+paint+s[last:]
p.write_bytes(s.replace('\n','\r\n').encode('utf-8'))
