from pathlib import Path
import re, sys
p = Path(sys.argv[1]) / 'voidImageViewer/src/viv.c'
s = p.read_text(encoding='utf-8')

def once(old, new):
    global s
    pattern = '\n'.join(re.escape(line) if line.strip() else r'[ \t]*' for line in old.split('\n'))
    matches = list(re.finditer(pattern, s))
    assert len(matches) == 1, (len(matches), old)
    m = matches[0]
    s = s[:m.start()] + new + s[m.end():]

def block(first, last, change):
    global s
    i = s.index(first)
    j = s.index(last, i + len(first)) if last else len(s)
    s = s[:i] + change(s[i:j]) + s[j:]

once('// a reply from the image load thread', '''typedef struct _viv_load_job_s
{
    HANDLE thread;
    wchar_t *filename;
    int render_wide, render_high;
    BOOL use_icm, use_orientation;
    BOOL prefetch;
    volatile LONG stop;
} _viv_load_job_t;

enum { _VIV_LOAD_KEEP_FIRST = 1, _VIV_LOAD_DISCARD };

// a reply from an image load thread''')
once('struct _viv_reply_s *next;\n\tDWORD type;',
     'struct _viv_reply_s *next;\n    _viv_load_job_t *job;\n\tDWORD type;')
once('typedef struct _viv_webp_s\n{\n\tDWORD wide;',
     'typedef struct _viv_webp_s\n{\n    _viv_load_job_t *job;\n\tDWORD wide;')
s = s.replace('_viv_reply_add(DWORD type,DWORD size,void *data)',
              '_viv_reply_add(_viv_load_job_t *job,DWORD type,DWORD size,void *data)')
once('static HANDLE _viv_load_image_thread = 0;',
     '''static _viv_load_job_t *_viv_active_load = NULL, *_viv_retired_load = NULL;
#define _viv_load_image_thread (_viv_active_load ? _viv_active_load->thread : NULL)
#define _viv_load_image_filename (_viv_active_load ? _viv_active_load->filename : NULL)''')
once('static wchar_t *_viv_load_image_filename = 0;\n','')
once('static int _viv_load_render_wide = 0;\nstatic int _viv_load_render_high = 0;\n','')
once('static _viv_frame_t *_viv_preload_frames = 0;', 'static _viv_preview_t _viv_preload_preview;\nstatic _viv_frame_t *_viv_preload_frames = 0;')

once('static void _viv_clear_loading_preload(void)\n{',
     '''static LONG _viv_load_stop_state(_viv_load_job_t *job)
{
    return InterlockedCompareExchange(&job->stop, 0, 0);
}

static void _viv_cancel_load(BOOL keep_first)
{
    _viv_load_image_allow_draw = keep_first;
    _viv_load_image_terminate = 1;
    if (_viv_active_load)
        InterlockedExchange(&_viv_active_load->stop, keep_first ? _VIV_LOAD_KEEP_FIRST : _VIV_LOAD_DISCARD);
}

static void _viv_release_load(_viv_load_job_t *job)
{
    if (job->thread) CloseHandle(job->thread);
    mem_free(job->filename);
    mem_free(job);
}

static void _viv_show_preview(_viv_preview_t *preview)
{
    viv_copy_current_image_to_last_image();
    _viv_clear();
    _viv_preview = *preview;
    preview->hbitmap = NULL;
    _viv_image_wide = _viv_preview.image_wide;
    _viv_image_high = _viv_preview.image_high;
    os_copy_memory(_viv_frame_fd, _viv_load_fd, sizeof(WIN32_FIND_DATA));
    _viv_start_first_frame();
    _viv_process_pending_clear();
}

static void _viv_clear_loading_preload(void)
{''')
block('static void _viv_clear_loading_preload(void)\n{', 'static void _viv_clear_preload(void)\n{', lambda t: t.replace(
    '_viv_load_image_terminate = 1;', '_viv_cancel_load(FALSE);').replace(
    'static void _viv_clear_preload_frames(void)\n{', '''static void _viv_clear_preload_frames(void)
{
    if (_viv_preload_preview.hbitmap)
    {
        DeleteObject(_viv_preload_preview.hbitmap);
        _viv_preload_preview.hbitmap = NULL;
    }'''))

once('''		_viv_load_image_allow_draw = 0;
		_viv_load_image_terminate = 1;
        _viv_load_image_allow_draw = 0;
        _viv_should_activate_preload_on_load = 0;''', '\t\t_viv_cancel_load(FALSE);')
once('''	// clear any existing preload and start a fresh one.
	_viv_clear_preload();

	if (_viv_load_image_thread)''',
'''	// clear any existing preload and start a fresh one.
	_viv_clear_preload();

    if (!is_preload && !_viv_is_slideshow && _viv_load_image_thread && !_viv_retired_load)
    {
        // Let the new selection start while a stale codec call finishes.
        _viv_cancel_load(FALSE);
        SetThreadPriority(_viv_load_image_thread, THREAD_PRIORITY_BELOW_NORMAL);
        _viv_retired_load = _viv_active_load;
        _viv_active_load = NULL;
    }

	if (_viv_active_load)''')
once('''        if (_viv_load_is_preload || _viv_preview.hbitmap) _viv_load_image_allow_draw = 0;
		_viv_load_image_terminate = 1;''',
'''        _viv_cancel_load(_viv_is_slideshow && !_viv_load_is_preload &&
            !_viv_preview.hbitmap && _viv_load_image_allow_draw);''')
once('''		DWORD thread_id;
//		int rw;
//		int rh;
		RECT rect;''', '''		DWORD thread_id;
		RECT rect;
        _viv_load_job_t *job = mem_alloc(sizeof(*job));''')
once('''//		_viv_get_render_size(&rw,&rh);

		_viv_load_image_allow_draw = 1;
		_viv_load_image_terminate = 0;

		if (_viv_load_image_filename)
		{
			mem_free(_viv_load_image_filename);
		}

		_viv_load_image_filename = string_alloc(fd->cFileName);'''.replace('\n\n\t\tif', '\n\t\t\n\t\tif').replace('\n\n\t\t_viv_load_image_filename','\n\t\t\n\t\t_viv_load_image_filename'),
'''        ZeroMemory(job, sizeof(*job));
        job->filename = string_alloc(fd->cFileName);
        job->use_icm = config_icm;
        job->use_orientation = config_orientation;
        job->prefetch = is_preload;
        _viv_active_load = job;
		_viv_load_image_allow_draw = 1;
		_viv_load_image_terminate = 0;''')
once('''		_viv_load_render_wide = rect.right - rect.left;
		_viv_load_render_high = rect.bottom - rect.top - _viv_get_status_high() - _viv_get_controls_high();''',
'''        job->render_wide = max(1, rect.right - rect.left);
        job->render_high = max(1, rect.bottom - rect.top - _viv_get_status_high() - _viv_get_controls_high());''')
once('''		_viv_load_image_thread = CreateThread(NULL,0,_viv_load_image_thread_proc,0,0,&thread_id);
        if (is_preload && _viv_load_image_thread)
            SetThreadPriority(_viv_load_image_thread, THREAD_PRIORITY_BELOW_NORMAL);''',
'''        job->thread = CreateThread(NULL, 0, _viv_load_image_thread_proc, job, 0, &thread_id);
        if (!job->thread)
            _viv_reply_add(job, _VIV_REPLY_LOAD_IMAGE_FAILED, 0, NULL);
        else if (is_preload)
            SetThreadPriority(job->thread, THREAD_PRIORITY_BELOW_NORMAL);''')

once('static void _viv_on_size(void)\n{',
     '''static BOOL _viv_open_pending_load(void)
{
    WIN32_FIND_DATA *fd = _viv_load_image_next_fd;
    if (!fd) return FALSE;
    _viv_load_image_next_fd = NULL;
    if (_viv_load_image_next_is_preload || string_compare(fd->cFileName, _viv_requested_fd.cFileName) == 0)
        _viv_open(fd, _viv_load_image_next_is_preload);
    mem_free(fd);
    return TRUE;
}

static void _viv_on_size(void)
{''')
once('static void _viv_exit(void)\n{\n\t_viv_load_image_terminate = 1;',
     'static void _viv_exit(void)\n{\n\t_viv_cancel_load(FALSE);')
once('''				next_e = e->next;

				switch(e->type)'''.replace('->next;\n\n','->next;\n\t\t\t\t\n'),
'''				next_e = e->next;
                if (e->job != _viv_active_load)
                {
                    if (e->job == _viv_retired_load &&
                        (e->type == _VIV_REPLY_LOAD_IMAGE_COMPLETE || e->type == _VIV_REPLY_LOAD_IMAGE_FAILED))
                    {
                        _viv_release_load(_viv_retired_load);
                        _viv_retired_load = NULL;
                        if (!_viv_open_pending_load() && !_viv_active_load)
                            _viv_preload_next();
                    }
                    _viv_reply_free(e);
                    e = next_e;
                    continue;
                }

				switch(e->type)''')
i = s.index('\t\t\t\t\t\t\tif (_viv_load_image_thread)', s.index('case _VIV_WM_REPLY:'))
j = s.index('\t\t\t\t\t\t\t_viv_status_update();', i)
s = s[:i] + '''                            _viv_release_load(_viv_active_load);
                            _viv_active_load = NULL;
                            _viv_open_pending_load();

''' + s[j:]
i = s.index('                    case _VIV_REPLY_LOAD_IMAGE_PREVIEW:')
j = s.index('\t\t\t\t\tcase _VIV_REPLY_LOAD_IMAGE_FIRST_FRAME:',i)
s = s[:i] + '''                    case _VIV_REPLY_LOAD_IMAGE_PREVIEW:
                    {
                        _viv_preview_t *preview = (_viv_preview_t *)(e + 1);
                        if (!_viv_load_image_terminate)
                        {
                            if (_viv_load_is_preload && !_viv_should_activate_preload_on_load)
                            {
                                _viv_preload_preview = *preview;
                                preview->hbitmap = NULL;
                            }
                            else if (string_compare(_viv_load_fd->cFileName, _viv_requested_fd.cFileName) == 0)
                            {
                                _viv_load_is_preload = 0;
                                _viv_should_activate_preload_on_load = 0;
                                _viv_clear_preload();
                                _viv_show_preview(preview);
                            }
                        }
                        break;
                    }

''' + s[j:]
once('''                                        _viv_load_image_allow_draw = 0;
                                        _viv_load_image_terminate = 1;''', '                                        _viv_cancel_load(FALSE);')
once('''                if(!_viv_frame_count) _viv_paint_empty(ps.hdc);
                else fyto_paint_image_hover(ps.hdc);''', '''                if (!_viv_frame_count && !_viv_preview.hbitmap) _viv_paint_empty(ps.hdc);
                else if (_viv_frame_count) fyto_paint_image_hover(ps.hdc);''')
i = s.index('\t// stop load_image immediately...')
j = s.index('\tif (_viv_last_frames)',i)
s = s[:i] + '''    // Both workers can still post replies until their codec calls return.
    _viv_cancel_load(FALSE);
    if (_viv_active_load)
    {
        if (_viv_load_image_thread) WaitForSingleObject(_viv_load_image_thread, INFINITE);
        _viv_release_load(_viv_active_load);
        _viv_active_load = NULL;
    }
    if (_viv_retired_load)
    {
        WaitForSingleObject(_viv_retired_load->thread, INFINITE);
        _viv_release_load(_viv_retired_load);
        _viv_retired_load = NULL;
    }

''' + s[j:]
once('static void _viv_preload_next(void)\n{\n    _viv_close_neighbor_watch();',
     '''static void _viv_preload_next(void)
{
    if (_viv_active_load || _viv_retired_load) return;
    _viv_close_neighbor_watch();''')
block('static void _viv_open_preload(void)\n{', None, lambda t: t.replace(
    '\t\tif (_viv_preload_frame_loaded_count)', '''        if (_viv_preload_preview.hbitmap)
        {
            _viv_load_is_preload = 0;
            _viv_should_activate_preload_on_load = 0;
            _viv_show_preview(&_viv_preload_preview);
            _viv_clear_preload();
        }
        else
\t\tif (_viv_preload_frame_loaded_count)''', 1))

def worker(t):
    t = t.replace('static void _viv_reply_still_frame(_viv_reply_load_image_first_frame_t *frame)',
        'static void _viv_reply_still_frame(_viv_load_job_t *job, _viv_reply_load_image_first_frame_t *frame)')
    t = t.replace('static BOOL _viv_load_fast_jpeg(IStream *stream,', 'static BOOL _viv_load_fast_jpeg(_viv_load_job_t *job, IStream *stream,')
    t = t.replace('static int _viv_webp_frame_proc(_viv_webp_t *viv_webp,BYTE *pixels,int delay)\n{',
        'static int _viv_webp_frame_proc(_viv_webp_t *viv_webp,BYTE *pixels,int delay)\n{\n    _viv_load_job_t *job = viv_webp->job;')
    t = t.replace('static DWORD WINAPI _viv_load_image_thread_proc(void *param)\n{',
        'static DWORD WINAPI _viv_load_image_thread_proc(void *param)\n{\n    _viv_load_job_t *job = param;')
    t = t.replace('viv_webp.screen_hdc = NULL;', 'viv_webp.job = job;\n\t\t\t\tviv_webp.screen_hdc = NULL;')
    t = t.replace('!_viv_load_is_preload && !_viv_load_image_terminate', '!_viv_load_stop_state(job)')
    t = t.replace('_viv_load_image_terminate && !_viv_load_image_allow_draw', '_viv_load_stop_state(job) == _VIV_LOAD_DISCARD')
    t = t.replace('!_viv_load_image_terminate || _viv_load_image_allow_draw', '_viv_load_stop_state(job) != _VIV_LOAD_DISCARD')
    t = t.replace('_viv_load_image_terminate', '_viv_load_stop_state(job)')
    t = t.replace('_viv_load_is_preload', 'job->prefetch')
    t = t.replace('_viv_load_image_filename', 'job->filename')
    t = t.replace('_viv_load_render_wide', 'job->render_wide')
    t = t.replace('_viv_load_render_high', 'job->render_high')
    t = t.replace('config_icm', 'job->use_icm')
    t = t.replace('config_orientation', 'job->use_orientation')
    t = t.replace('_viv_reply_still_frame(frame)', '_viv_reply_still_frame(job, frame)')
    t = t.replace('_viv_reply_still_frame(&first_frame)', '_viv_reply_still_frame(job, &first_frame)')
    t = t.replace('_viv_load_fast_jpeg(stream,', '_viv_load_fast_jpeg(job, stream,')
    t = t.replace('_viv_reply_add(', '_viv_reply_add(job, ')
    t = t.replace('''	_viv_reply_add(job, ret ? _VIV_REPLY_LOAD_IMAGE_COMPLETE : _VIV_REPLY_LOAD_IMAGE_FAILED,0,0);

	CoUninitialize();'''.replace(',0,0);\n\n', ',0,0);\n\t\n'),
        '''	CoUninitialize();

	_viv_reply_add(job, ret ? _VIV_REPLY_LOAD_IMAGE_COMPLETE : _VIV_REPLY_LOAD_IMAGE_FAILED,0,0);''')
    return t
block('static int _viv_webp_frame_proc(_viv_webp_t *viv_webp,BYTE *pixels,int delay)\n{',
      'static void _viv_reply_free(_viv_reply_t *e)\n{', worker)
once('\te->type = type;\n\te->size = size;', '\te->job = job;\n\te->type = type;\n\te->size = size;')

p.write_bytes(s.replace('\n', '\r\n').encode('utf-8'))
