from pathlib import Path
import sys
p=Path(sys.argv[1])/'voidImageViewer/src/viv.c'
raw=p.read_bytes()
crlf=b'\r\n' in raw
s=raw.decode('utf-8').replace('\r\n','\n')
def swap(a,b,n=1):
    global s
    assert s.count(a)==n,(a,s.count(a))
    s=s.replace(a,b)

swap('static void _viv_schedule_directory_scan(void);',
'''static void _viv_schedule_directory_scan(void);
static void _viv_replay_directory_navigation(BOOL fresh);''')
swap('''    size_t count;
    BYTE sort, ascending;
    volatile LONG cancelled;
    BOOL ready;
} _viv_directory_t;

static _viv_directory_t *_viv_directory_scan, *_viv_directory_cache;''',
'''    size_t count;
    BYTE sort, ascending;
    volatile LONG cancelled;
    BOOL ready, limited;
    DWORD completed_at;
    WIN32_FIND_DATA origin, before, after, first, last;
} _viv_directory_t;

static _viv_directory_t *_viv_directory_scan, *_viv_directory_cache;
static BOOL _viv_directory_changed(const _viv_directory_t *directory);
enum { FYTO_NAV_PREVIOUS = 1, FYTO_NAV_WRAP = 2 };
static BYTE _viv_directory_steps[128];
static unsigned _viv_directory_step_count;''')
swap('static void _viv_cancel_load(BOOL keep_first)\n{',
'''static void _viv_cancel_thread_io(HANDLE thread)
{
    typedef BOOL (WINAPI *cancel_io_t)(HANDLE);
    static cancel_io_t cancel_io;
    static BOOL initialized;
    if (!initialized)
    {
        cancel_io = (cancel_io_t)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "CancelSynchronousIo");
        initialized = TRUE;
    }
    if (cancel_io && thread) cancel_io(thread);
}

static void _viv_cancel_load(BOOL keep_first)
{''')
swap('''    if (_viv_active_load)
        InterlockedExchange(&_viv_active_load->stop, keep_first ? _VIV_LOAD_KEEP_FIRST : _VIV_LOAD_DISCARD);''',
'''    if (_viv_active_load)
    {
        InterlockedExchange(&_viv_active_load->stop, keep_first ? _VIV_LOAD_KEEP_FIRST : _VIV_LOAD_DISCARD);
        if (!keep_first) _viv_cancel_thread_io(_viv_active_load->thread);
    }''')
swap('''    if(!is_preload){os_copy_memory(&_viv_requested_fd,fd,sizeof(WIN32_FIND_DATA));if(_viv_load_image_next_fd){mem_free(_viv_load_image_next_fd);_viv_load_image_next_fd=NULL;}}''',
'''    if (!is_preload)
    {
        if (!_viv_same_image(fd, &_viv_requested_fd)) _viv_directory_step_count = 0;
        os_copy_memory(&_viv_requested_fd, fd, sizeof(WIN32_FIND_DATA));
        if (_viv_load_image_next_fd)
        {
            mem_free(_viv_load_image_next_fd);
            _viv_load_image_next_fd = NULL;
        }
    }''')
i=s.index('        case FYTO_DIRECTORY_READY:')
j=s.index('        case WM_DISPLAYCHANGE:',i)
s=s[:i]+'''        case FYTO_DIRECTORY_READY:
            if (_viv_directory_scan)
            {
                _viv_directory_t *scan = _viv_directory_scan;
                _viv_directory_scan = NULL;
                if (scan->cancelled)
                    _viv_free_directory(scan);
                else
                {
                    _viv_free_directory(_viv_directory_cache);
                    _viv_directory_cache = scan;
                    if (!scan->ready)
                    {
                        _viv_directory_step_count = 0;
                        return 0;
                    }
                }
                _viv_replay_directory_navigation(TRUE);
                if (!_viv_directory_step_count && !_viv_directory_scan &&
                    (!_viv_directory_cache || !_viv_directory_changed(_viv_directory_cache)))
                    _viv_preload_next();
            }
            return 0;
'''+s[j:]

swap('static BOOL _viv_cached_directory_next(const WIN32_FIND_DATA *current, int prev, BOOL wrap, WIN32_FIND_DATA *next)',
'''static BOOL _viv_directory_changed(const _viv_directory_t *directory)
{
    if (directory->changes != INVALID_HANDLE_VALUE)
        return WaitForSingleObject(directory->changes, 0) != WAIT_TIMEOUT;
    return GetTickCount() - directory->completed_at >= 750;
}

static BOOL _viv_cached_directory_next(const WIN32_FIND_DATA *current, int prev, BOOL wrap, WIN32_FIND_DATA *next, BOOL fresh)''')
swap('    if (WaitForSingleObject(directory->changes, 0) != WAIT_TIMEOUT)\n    {',
     '    if (!fresh && _viv_directory_changed(directory))\n    {')
swap('''    leaf.dwReserved0 = leaf.dwReserved1 = 0;
    if (directory->sort == config_nav_sort''',
'''    leaf.dwReserved0 = leaf.dwReserved1 = 0;
    if (directory->limited)
    {
        if (!_viv_same_image(&leaf, &directory->origin) || directory->sort != config_nav_sort ||
            directory->ascending != config_nav_sort_ascending) return FALSE;
        best = prev ? &directory->before : &directory->after;
        if (!*best->cFileName && wrap) best = prev ? &directory->last : &directory->first;
        if (!*best->cFileName) best = NULL;
    }
    else if (directory->sort == config_nav_sort''')

swap('static DWORD WINAPI _viv_scan_directory(void *arg)',
'''static void _viv_record_directory_neighbor(_viv_directory_t *scan, const WIN32_FIND_DATA *file)
{
    int order = _viv_fd_compare_order(file, &scan->origin, scan->sort, scan->ascending);
    if (!order) return;
    if (order < 0 && (!*scan->before.cFileName ||
        _viv_fd_compare_order(file, &scan->before, scan->sort, scan->ascending) > 0)) scan->before = *file;
    if (order > 0 && (!*scan->after.cFileName ||
        _viv_fd_compare_order(file, &scan->after, scan->sort, scan->ascending) < 0)) scan->after = *file;
    if (!*scan->first.cFileName ||
        _viv_fd_compare_order(file, &scan->first, scan->sort, scan->ascending) < 0) scan->first = *file;
    if (!*scan->last.cFileName ||
        _viv_fd_compare_order(file, &scan->last, scan->sort, scan->ascending) > 0) scan->last = *file;
}

static DWORD WINAPI _viv_scan_directory(void *arg)''')
i=s.index('    if (scan->changes != INVALID_HANDLE_VALUE)',s.index('static DWORD WINAPI _viv_scan_directory'))
j=s.index('    if (!scan->ready)',i)
s=s[:i]+'''    if (!InterlockedCompareExchange(&scan->cancelled, 0, 0))
    {
        scan->changes = FindFirstChangeNotificationW(scan->folder, FALSE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE |
            FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION);
    }

    string_copy(pattern, scan->folder);
    string_cat_utf8(pattern, (const utf8_t *)"\\\\*.*");
    search = INVALID_HANDLE_VALUE;
    if (!InterlockedCompareExchange(&scan->cancelled, 0, 0))
        search = FindFirstFileExW(pattern, FindExInfoBasic, &fd, FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);
    if (search == INVALID_HANDLE_VALUE && !InterlockedCompareExchange(&scan->cancelled, 0, 0))
        search = FindFirstFileW(pattern, &fd);
    if (search != INVALID_HANDLE_VALUE)
    {
        for (;;)
        {
            if (InterlockedCompareExchange(&scan->cancelled, 0, 0)) break;
            if (_viv_is_valid_filename(&fd))
            {
                fd.dwReserved0 = fd.dwReserved1 = 0;
                if (!scan->limited && scan->count == capacity)
                {
                    size_t wanted = capacity ? capacity * 2 : 256;
                    WIN32_FIND_DATA *files = wanted <= 65536
                        ? realloc(scan->files, wanted * sizeof(*files)) : NULL;
                    if (files)
                    {
                        scan->files = files;
                        capacity = wanted;
                    }
                    else
                    {
                        size_t i;
                        scan->limited = TRUE;
                        for (i = 0; i < scan->count; ++i)
                            _viv_record_directory_neighbor(scan, &scan->files[i]);
                        free(scan->files);
                        scan->files = NULL;
                        scan->count = 0;
                    }
                }
                if (scan->limited) _viv_record_directory_neighbor(scan, &fd);
                else scan->files[scan->count++] = fd;
            }
            if (!FindNextFileW(search, &fd))
            {
                scan->ready = GetLastError() == ERROR_NO_MORE_FILES;
                break;
            }
        }
        FindClose(search);
    }
    else if (GetLastError() == ERROR_FILE_NOT_FOUND)
        scan->ready = TRUE;
'''+s[j:]
swap('''        qsort(scan->files, scan->count, sizeof(*scan->files), _viv_directory_compare_names);
#endif
    }
    PostMessageW(scan->hwnd, FYTO_DIRECTORY_READY, 0, 0);''',
'''        qsort(scan->files, scan->count, sizeof(*scan->files), _viv_directory_compare_names);
        scan->sort = CONFIG_NAV_SORT_NAME;
        scan->ascending = TRUE;
#endif
    }
    scan->completed_at = GetTickCount();
    PostMessageW(scan->hwnd, FYTO_DIRECTORY_READY, 0, 0);''')
i=s.index('static void _viv_schedule_directory_scan(void)\n{')
j=s.index('// Fit the borderless window',i)
s=s[:i]+'''static void _viv_schedule_directory_scan(void)
{
    wchar_t folder[STRING_SIZE];
    _viv_directory_t *scan;
    const WIN32_FIND_DATA *current = _viv_current_fd;
    if ((!config_preload_next && !_viv_directory_step_count) ||
        _viv_playlist_start || _viv_random || !*current->cFileName) return;
    string_get_path_part(folder, current->cFileName);
    if (_viv_directory_scan)
    {
        if (string_compare(folder, _viv_directory_scan->folder) != 0)
        {
            InterlockedExchange(&_viv_directory_scan->cancelled, 1);
            _viv_cancel_thread_io(_viv_directory_scan->thread);
        }
        return;
    }

    if (_viv_directory_cache && string_compare(folder, _viv_directory_cache->folder) == 0 &&
        _viv_directory_cache->ready && !_viv_directory_changed(_viv_directory_cache))
    {
        const _viv_directory_t *cached = _viv_directory_cache;
        BOOL same_order = cached->sort == config_nav_sort && cached->ascending == config_nav_sort_ascending;
        if (!cached->limited && same_order) return;
#ifndef _MSC_VER
        if (!cached->limited) return;
#endif
        if (cached->limited && same_order && !_viv_directory_step_count) return;
    }

    scan = mem_alloc(sizeof(*scan));
    ZeroMemory(scan, sizeof(*scan));
    scan->hwnd = _viv_hwnd;
    scan->sort = config_nav_sort;
    scan->ascending = config_nav_sort_ascending;
    scan->origin = *current;
    string_copy(scan->origin.cFileName, string_get_filename_part(current->cFileName));
    scan->origin.dwReserved0 = scan->origin.dwReserved1 = 0;
    string_copy(scan->folder, folder);
    scan->changes = INVALID_HANDLE_VALUE;
    scan->thread = CreateThread(NULL, 0, _viv_scan_directory, scan, 0, NULL);
    if (!scan->thread)
    {
        _viv_free_directory(scan);
        _viv_directory_step_count = 0;
        return;
    }
    _viv_directory_scan = scan;
    SetThreadPriority(scan->thread, THREAD_PRIORITY_BELOW_NORMAL);
}

static void _viv_replay_directory_navigation(BOOL fresh)
{
    WIN32_FIND_DATA current = *_viv_current_fd;
    unsigned used, remaining;
    BOOL moved = FALSE;

    for (used = 0; used < _viv_directory_step_count; used++)
    {
        WIN32_FIND_DATA next;
        BYTE step = _viv_directory_steps[used];
        int prev = (step & FYTO_NAV_PREVIOUS) != 0;
        if (!_viv_cached_directory_next(&current, prev, (step & FYTO_NAV_WRAP) != 0, &next, fresh)) break;
        _viv_last_is_prev = prev;
        if (*next.cFileName)
        {
            current = next;
            moved = TRUE;
        }
    }
    remaining = _viv_directory_step_count - used;
    if (used)
    {
        memmove(_viv_directory_steps, _viv_directory_steps + used, remaining);
        if (moved) _viv_open(&current, 0);
        _viv_directory_step_count = remaining;
    }
    if (remaining) _viv_schedule_directory_scan();
}

static void _viv_next_manual(int prev, int repeat)
{
    if (_viv_load_is_preload || _viv_preview.hbitmap) repeat = 0;
    if (_viv_playlist_start || _viv_random || !*_viv_current_fd->cFileName)
    {
        _viv_next(prev, 1, 0, repeat);
        return;
    }
    if (repeat && (_viv_directory_step_count ||
        (_viv_load_image_thread && (_viv_load_frame_count <= 1 || _viv_load_image_terminate))))
        return;

    if (_viv_directory_step_count == ARRAYSIZE(_viv_directory_steps)) _viv_directory_step_count--;
    _viv_directory_steps[_viv_directory_step_count++] =
        (prev ? FYTO_NAV_PREVIOUS : 0) | (_viv_wheel_navigation ? 0 : FYTO_NAV_WRAP);
    _viv_replay_directory_navigation(FALSE);
    if (_viv_is_slideshow)
    {
        KillTimer(_viv_hwnd, VIV_ID_SLIDESHOW_TIMER);
        SetTimer(_viv_hwnd, VIV_ID_SLIDESHOW_TIMER, config_slideshow_rate, 0);
    }
}

static void _viv_preload_next(void)
{
    WIN32_FIND_DATA next;
    if (!config_preload_next || _viv_active_load || _viv_retired_load || _viv_directory_step_count) return;
    if (!_viv_playlist_start && !_viv_random && *_viv_current_fd->cFileName)
    {
        if (_viv_cached_directory_next(_viv_current_fd, _viv_last_is_prev, TRUE, &next, FALSE))
        {
            if (*next.cFileName) _viv_open(&next, 1);
        }
        else
            _viv_schedule_directory_scan();
        return;
    }
    _viv_next(_viv_last_is_prev, 0, 1, 0);
}

'''+s[j:]

swap('        if (cached.hbitmap) _viv_show_preview(&cached, _viv_current_fd);',
'''        if (cached.hbitmap) _viv_show_preview(&cached, _viv_current_fd);
        else _viv_schedule_directory_scan();''')

i=s.index('    // Both workers can still post replies until their codec calls return.')
j=s.index('    _viv_free_directory(_viv_directory_cache);',i)
s=s[:i]+'''    // Cancel pending reads before releasing resources shared with the workers.
    _viv_cancel_load(FALSE);
    if (_viv_retired_load) _viv_cancel_thread_io(_viv_retired_load->thread);
    if (_viv_directory_scan)
    {
        InterlockedExchange(&_viv_directory_scan->cancelled, 1);
        _viv_cancel_thread_io(_viv_directory_scan->thread);
    }

    if ((_viv_load_image_thread && WaitForSingleObject(_viv_load_image_thread, 250) != WAIT_OBJECT_0) ||
        (_viv_retired_load && WaitForSingleObject(_viv_retired_load->thread, 250) != WAIT_OBJECT_0) ||
        (_viv_directory_scan && WaitForSingleObject(_viv_directory_scan->thread, 250) != WAIT_OBJECT_0))
    {
        // The exit command has saved settings. A stalled driver must not keep the window open.
        TerminateProcess(GetCurrentProcess(), 0);
        return;
    }
    if (_viv_active_load)
    {
        _viv_release_load(_viv_active_load);
        _viv_active_load = NULL;
    }
    if (_viv_retired_load)
    {
        _viv_release_load(_viv_retired_load);
        _viv_retired_load = NULL;
    }
    _viv_free_directory(_viv_directory_scan);
'''+s[j:]
p.write_bytes((s.replace('\n','\r\n') if crlf else s).encode('utf-8'))
