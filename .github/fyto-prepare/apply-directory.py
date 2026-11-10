from pathlib import Path
import sys
f = Path(sys.argv[1]) / 'voidImageViewer/src/viv.c'
s = f.read_text(encoding='utf-8')
def swap(old,new):
    global s
    assert s.count(old) == 1, old
    s = s.replace(old,new)

swap('static void _viv_preload_next(void);', '''static void _viv_preload_next(void);
static void _viv_schedule_directory_scan(void);''')
swap('''static struct
{
    HANDLE changes;
    WIN32_FIND_DATA from, to;
    int previous, sort, ascending;
    BOOL ready;
} _viv_neighbor;''', '''typedef struct _viv_directory_s
{
    HANDLE thread, changes;
    HWND hwnd;
    wchar_t folder[STRING_SIZE];
    WIN32_FIND_DATA *files;
    size_t count;
    volatile LONG cancelled;
    BOOL ready;
} _viv_directory_t;

static _viv_directory_t *_viv_directory_scan, *_viv_directory_cache;''')
swap('#define FYTO_REFIT_WINDOW (WM_APP+26)', '#define FYTO_REFIT_WINDOW (WM_APP+26)\n#define FYTO_DIRECTORY_READY (WM_APP+27)')
swap('''    _viv_start_first_frame();
    _viv_process_pending_clear();
}

static void _viv_clear_loading_preload''', '''    _viv_start_first_frame();
    _viv_process_pending_clear();
    _viv_schedule_directory_scan();
}

static void _viv_clear_loading_preload''')
a = s.index('static void _viv_close_neighbor_watch(void)\n{')
b = s.index('    if(is_preload &&',a)
s = s[:a] + '''static void _viv_free_directory(_viv_directory_t *directory)
{
    if (!directory) return;
    if (directory->thread) CloseHandle(directory->thread);
    if (directory->changes && directory->changes != INVALID_HANDLE_VALUE)
        FindCloseChangeNotification(directory->changes);
    free(directory->files);
    mem_free(directory);
}

static void _viv_open(WIN32_FIND_DATA *fd,int is_preload)
{
''' + s[b:]
swap('''        case FYTO_REFIT_WINDOW:
            _viv_fit_image_window();return 0;''', '''        case FYTO_REFIT_WINDOW:
            _viv_fit_image_window();return 0;
        case FYTO_DIRECTORY_READY:
            if (_viv_directory_scan)
            {
                _viv_directory_t *scan = _viv_directory_scan;
                WaitForSingleObject(scan->thread, INFINITE);
                CloseHandle(scan->thread);
                scan->thread = NULL;
                _viv_directory_scan = NULL;
                if (scan->cancelled)
                    _viv_free_directory(scan);
                else
                {
                    _viv_free_directory(_viv_directory_cache);
                    _viv_directory_cache = scan;
                }
                _viv_preload_next();
            }
            return 0;''')
swap('''    _viv_close_neighbor_watch();
\tos_kill();''', '''    if (_viv_directory_scan)
    {
        InterlockedExchange(&_viv_directory_scan->cancelled, 1);
        WaitForSingleObject(_viv_directory_scan->thread, INFINITE);
        _viv_free_directory(_viv_directory_scan);
    }
    _viv_free_directory(_viv_directory_cache);
\tos_kill();''')

i = s.index('static void _viv_next_manual(int prev, int repeat)\n{')
j = s.index('// Fit the borderless window',i)
s = s[:i] + '''static BOOL _viv_cached_directory_next(const WIN32_FIND_DATA *current, int prev, BOOL wrap, WIN32_FIND_DATA *next)
{
    _viv_directory_t *directory = _viv_directory_cache;
    const WIN32_FIND_DATA *best = NULL, *first = NULL;
    WIN32_FIND_DATA leaf;
    wchar_t folder[STRING_SIZE], path[STRING_SIZE];
    size_t i;
    int direction = prev ? -1 : 1;
    if (!directory || !directory->ready || !*current->cFileName ||
        WaitForSingleObject(directory->changes, 0) != WAIT_TIMEOUT) return FALSE;
    string_get_path_part(folder, current->cFileName);
    if (string_compare(folder, directory->folder) != 0) return FALSE;

    leaf = *current;
    string_copy(leaf.cFileName, string_get_filename_part(current->cFileName));
    leaf.dwReserved0 = leaf.dwReserved1 = 0;
    for (i = 0; i < directory->count; ++i)
    {
        const WIN32_FIND_DATA *file = &directory->files[i];
        if (string_compare(file->cFileName, leaf.cFileName) == 0) continue;
        if (_viv_fd_compare(file, &leaf) * direction > 0 &&
            (!best || _viv_fd_compare(file, best) * direction < 0)) best = file;
        if (wrap && (!first || _viv_fd_compare(file, first) * direction < 0)) first = file;
    }
    if (!best) best = first;
    next->cFileName[0] = 0;
    if (best)
    {
        *next = *best;
        string_path_combine(path, folder, best->cFileName);
        string_copy(next->cFileName, path);
    }
    return TRUE;
}

static DWORD WINAPI _viv_scan_directory(void *arg)
{
    _viv_directory_t *scan = arg;
    WIN32_FIND_DATA fd;
    HANDLE search;
    wchar_t pattern[STRING_SIZE];
    size_t capacity = 0;

    scan->changes = FindFirstChangeNotificationW(scan->folder, FALSE,
        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE |
        FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION);
    if (scan->changes != INVALID_HANDLE_VALUE)
    {
        string_copy(pattern, scan->folder);
        string_cat_utf8(pattern, (const utf8_t *)"\\\\*.*");
        search = FindFirstFileExW(pattern, FindExInfoBasic, &fd, FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);
        if (search == INVALID_HANDLE_VALUE) search = FindFirstFileW(pattern, &fd);
        if (search != INVALID_HANDLE_VALUE)
        {
            for (;;)
            {
                if (InterlockedCompareExchange(&scan->cancelled, 0, 0)) break;
                if (_viv_is_valid_filename(&fd))
                {
                    if (scan->count == capacity)
                    {
                        WIN32_FIND_DATA *files;
                        if (capacity == 65536) break;
                        capacity = capacity ? capacity * 2 : 256;
                        files = realloc(scan->files, capacity * sizeof(*files));
                        if (!files) break;
                        scan->files = files;
                    }
                    fd.dwReserved0 = fd.dwReserved1 = 0;
                    scan->files[scan->count++] = fd;
                }
                if (!FindNextFileW(search, &fd))
                {
                    scan->ready = GetLastError() == ERROR_NO_MORE_FILES &&
                        WaitForSingleObject(scan->changes, 0) == WAIT_TIMEOUT;
                    break;
                }
            }
            FindClose(search);
        }
    }
    PostMessageW(scan->hwnd, FYTO_DIRECTORY_READY, 0, 0);
    return 0;
}

static void _viv_schedule_directory_scan(void)
{
    wchar_t folder[STRING_SIZE];
    _viv_directory_t *scan;
    if (!config_preload_next || _viv_playlist_start || _viv_random || !*_viv_current_fd->cFileName) return;
    string_get_path_part(folder, _viv_current_fd->cFileName);
    if (_viv_directory_scan)
    {
        if (string_compare(folder, _viv_directory_scan->folder) != 0)
            InterlockedExchange(&_viv_directory_scan->cancelled, 1);
        return;
    }
    if (_viv_directory_cache && string_compare(folder, _viv_directory_cache->folder) == 0 &&
        (!_viv_directory_cache->ready || WaitForSingleObject(_viv_directory_cache->changes, 0) == WAIT_TIMEOUT))
        return;

    scan = mem_alloc(sizeof(*scan));
    ZeroMemory(scan, sizeof(*scan));
    scan->hwnd = _viv_hwnd;
    string_copy(scan->folder, folder);
    scan->thread = CreateThread(NULL, 0, _viv_scan_directory, scan, 0, NULL);
    if (!scan->thread)
    {
        _viv_free_directory(scan);
        return;
    }
    _viv_directory_scan = scan;
    SetThreadPriority(scan->thread, THREAD_PRIORITY_BELOW_NORMAL);
}

static void _viv_next_manual(int prev, int repeat)
{
    const WIN32_FIND_DATA *current = _viv_wheel_navigation && *_viv_requested_fd.cFileName
        ? &_viv_requested_fd : _viv_current_fd;
    WIN32_FIND_DATA next;
    if (_viv_load_is_preload || _viv_preview.hbitmap) repeat = 0;

    if (config_preload_next && !_viv_playlist_start && !_viv_random &&
        (!repeat || !_viv_load_image_thread || (_viv_load_frame_count > 1 && !_viv_load_image_terminate)) &&
        _viv_cached_directory_next(current, prev, !_viv_wheel_navigation, &next))
    {
        _viv_last_is_prev = prev;
        if (*next.cFileName) _viv_open(&next, 0);
        if (_viv_is_slideshow)
        {
            KillTimer(_viv_hwnd, VIV_ID_SLIDESHOW_TIMER);
            SetTimer(_viv_hwnd, VIV_ID_SLIDESHOW_TIMER, config_slideshow_rate, 0);
        }
    }
    else
        _viv_next(prev, 1, 0, repeat);
}

static void _viv_preload_next(void)
{
    WIN32_FIND_DATA next;
    if (!config_preload_next || _viv_active_load || _viv_retired_load) return;
    if (!_viv_playlist_start && !_viv_random && *_viv_current_fd->cFileName)
    {
        if (_viv_cached_directory_next(_viv_current_fd, _viv_last_is_prev, TRUE, &next))
        {
            if (*next.cFileName) _viv_open(&next, 1);
            return;
        }
        _viv_schedule_directory_scan();
        if (_viv_directory_scan) return;
    }
    _viv_next(_viv_last_is_prev, 0, 1, 0);
}

''' + s[j:]
f.write_bytes(s.replace('\n','\r\n').encode('utf-8'))
