from pathlib import Path
import sys
root=Path(sys.argv[1])
p=root/'voidImageViewer/src/viv.c'
raw=p.read_bytes()
crlf=b'\r\n' in raw
s=raw.decode('utf-8').replace('\r\n','\n')
def swap(a,b,n=1):
    global s
    assert s.count(a)==n,(a,s.count(a))
    s=s.replace(a,b)

swap('    BOOL prefetch, have_preview;\n    volatile LONG stop;',
'''    BOOL prefetch, have_preview, preview_only;
    UINT64 prefetch_pixels;
    volatile LONG stop, promoted;''')
swap('static void _viv_schedule_directory_scan(void);',
'''static void _viv_schedule_directory_scan(void);
static HBITMAP _viv_make_half_bitmap(HBITMAP source, int wide, int high, int half_wide, int half_high, BOOL top_down);''')
swap('} _viv_recent_previews[4];','} _viv_recent_previews[2];')
swap('// Budget 600 MiB for the previous and next decoded images combined.\n',
'''// Allow a small full bitmap on each side of the current image.
#define FYTO_NEIGHBOR_PIXELS (3ull * 1024 * 1024)
''')
swap('pixels = (300ull * 1024 * 1024 / 4) / frames;',
     'pixels = FYTO_NEIGHBOR_PIXELS / frames;')
swap('static BOOL _viv_load_discarded(void *context)',
'''static BOOL _viv_loading_in_background(_viv_load_job_t *job)
{
    return job->prefetch && !InterlockedCompareExchange(&job->promoted, 0, 0);
}

static BOOL _viv_load_discarded(void *context)''')
swap('''        job->prefetch = is_preload;
        job->have_preview''','''        job->prefetch = is_preload;
        if (is_preload)
        {
            const UINT64 budget = 224ull * 1024 * 1024 / 4;
            UINT64 pixels = (UINT64)_viv_image_wide * _viv_image_high;
            UINT frames = max(1, _viv_frame_count);
            job->prefetch_pixels = pixels <= budget / frames ? budget - pixels * frames : 0;
        }
        job->have_preview''')
swap('''							int allow_preload_next;
							
							allow_preload_next = 0;''',
'''							int allow_preload_next;

                            if (_viv_active_load->preview_only)
                            {
                                WIN32_FIND_DATA file = *_viv_load_fd;
                                BOOL selected = !_viv_load_image_terminate &&
                                    _viv_same_image(&file, &_viv_requested_fd);
                                _viv_release_load(_viv_active_load);
                                _viv_active_load = NULL;
                                if (selected || _viv_load_image_terminate || !_viv_preload_preview.hbitmap)
                                {
                                    _viv_clear_preload();
                                    _viv_load_is_preload = 0;
                                }
                                else
                                    _viv_preload_state = 1;
                                if (!_viv_open_pending_load() && selected)
                                    _viv_open(&file, 0);
                                _viv_status_update();
                                break;
                            }

							allow_preload_next = 0;''')
swap('static BOOL _viv_reply_still_frame(_viv_load_job_t *job, _viv_reply_load_image_first_frame_t *frame)',
'''static BOOL _viv_reply_reduced_bitmap(_viv_load_job_t *job, _viv_reply_load_image_first_frame_t *frame)
{
    _viv_preview_t preview = {frame->frame.hbitmap, frame->wide, frame->high, frame->wide, frame->high};
    frame->frame.hbitmap = NULL;
    job->preview_only = TRUE;
    while (preview.hbitmap && !_viv_load_discarded(job) &&
           (UINT64)preview.wide * preview.high > FYTO_NEIGHBOR_PIXELS)
    {
        UINT wide = preview.wide / 2 + (preview.wide & 1);
        UINT high = preview.high / 2 + (preview.high & 1);
        HBITMAP reduced = _viv_make_half_bitmap(preview.hbitmap, preview.wide, preview.high, wide, high, TRUE);
        DeleteObject(preview.hbitmap);
        preview.hbitmap = reduced;
        preview.wide = wide;
        preview.high = high;
    }
    if (preview.hbitmap && !_viv_load_discarded(job))
    {
        _viv_reply_add(job, _VIV_REPLY_LOAD_IMAGE_PREVIEW, sizeof(preview), &preview);
        return TRUE;
    }
    if (preview.hbitmap) DeleteObject(preview.hbitmap);
    return TRUE;
}

static BOOL _viv_reply_still_frame(_viv_load_job_t *job, _viv_reply_load_image_first_frame_t *frame)''')
swap('''    int mip_wide, mip_high;
    if (!_viv_load_discarded(job))
    {
        frame->frame_count = 1;''',
'''    int mip_wide, mip_high;
    UINT render_wide = job->render_wide, render_high = job->render_high;
    if (_viv_loading_in_background(job) && !_viv_cache_image_fits(frame->wide, frame->high, 1))
        return _viv_reply_reduced_bitmap(job, frame);
    if (!_viv_load_discarded(job))
    {
        UINT wide = frame->wide, high = frame->high;
        while ((UINT64)wide * high > FYTO_NEIGHBOR_PIXELS)
        {
            wide = wide / 2 + (wide & 1);
            high = high / 2 + (high & 1);
        }
        render_wide = min(render_wide, wide);
        render_high = min(render_high, high);
        frame->frame_count = 1;''')
swap('''            job->render_wide, job->render_high, &mip_wide, &mip_high, &frame->frame.mipmap, TRUE);''',
'''            render_wide, render_high, &mip_wide, &mip_high, &frame->frame.mipmap, TRUE);''')
swap('''        int orientation = job->use_orientation ? fyto_jpeg_orientation(&jpeg) : 0;''',
'''        BOOL reduce = _viv_loading_in_background(job) && !_viv_cache_image_fits(jpeg.wide, jpeg.high, 1);
        int orientation = job->use_orientation ? fyto_jpeg_orientation(&jpeg) : 0;''')
swap('''        if (!job->have_preview && !_viv_load_stop_state(job))''',
'''        if (!job->have_preview && !_viv_load_stop_state(job) &&
            (reduce || (UINT64)jpeg.wide * jpeg.high >= 8000000))''')
swap('''                    else _viv_reply_add(job, _VIV_REPLY_LOAD_IMAGE_PREVIEW, sizeof(preview), &preview);''',
'''                    else
                    {
                        if (reduce && _viv_loading_in_background(job))
                        {
                            job->preview_only = TRUE;
                            loaded = TRUE;
                        }
                        _viv_reply_add(job, _VIV_REPLY_LOAD_IMAGE_PREVIEW, sizeof(preview), &preview);
                    }''')
swap('''        if (_viv_load_stop_state(job) != _VIV_LOAD_DISCARD)
        {
            frame->frame.hbitmap = fyto_jpeg_full''',
'''        if (!loaded && reduce && _viv_loading_in_background(job))
        {
            job->preview_only = TRUE;
            loaded = TRUE;
        }
        if (!loaded && !_viv_load_discarded(job))
        {
            frame->frame.hbitmap = fyto_jpeg_full''')
swap('    int png_flags = -1;', '    int png_flags = -1;\n    UINT64 png_pixels = 0;')
swap('''                        png_flags = fyto_png_flags((const BYTE *)buf, size, job->use_orientation, job->use_icm);''',
'''                        png_flags = fyto_png_flags((const BYTE *)buf, size, job->use_orientation, job->use_icm);
                        if (size >= 24 && !memcmp(buf, "\\211PNG\\r\\n\\032\\n", 8) && !memcmp(buf + 12, "IHDR", 4))
                            png_pixels = (UINT64)fyto_png_u32((const BYTE *)buf + 16) *
                                fyto_png_u32((const BYTE *)buf + 20);''')
swap('''        if (jpeg_file && _viv_load_fast_jpeg(job, stream, &orientation, &first_frame))
            ret = 1;''',
'''        if (png_pixels > job->prefetch_pixels && _viv_loading_in_background(job))
        {
            job->preview_only = TRUE;
            ret = 1;
        }
        else if (jpeg_file && _viv_load_fast_jpeg(job, stream, &orientation, &first_frame))
            ret = 1;''')
swap('''								// get frame delays.
								if (first_frame.frame_count > 1)''',
'''                                if (_viv_loading_in_background(job) &&
                                    !_viv_cache_image_fits(first_frame.wide, first_frame.high, first_frame.frame_count))
                                {
                                    job->preview_only = TRUE;
                                    ret = 1;
                                    goto dispose_gdi_image;
                                }

								// get frame delays.
								if (first_frame.frame_count > 1)''')
swap('''				os_GdipDisposeImage(image);
			}
			else
			{
				_viv_webp_t viv_webp;''',
'''dispose_gdi_image:
				os_GdipDisposeImage(image);
			}
			else
			{
				_viv_webp_t viv_webp;''')
swap('''	viv_webp->frame_count = frame_count;

	viv_webp->screen_hdc''',
'''	viv_webp->frame_count = frame_count;
    if (_viv_loading_in_background(viv_webp->job) && !_viv_cache_image_fits(wide, high, frame_count))
    {
        viv_webp->job->preview_only = TRUE;
        return 0;
    }

	viv_webp->screen_hdc''')
i=s.index('// Keep a ready viewport image when the full-size previous image is evicted.')
j=s.index('// copy the current image to the last image.',i)
s=s[:i]+'''// Keep a small, already rendered bitmap when the full image will be freed.
static void _viv_keep_current_preview(void)
{
    UINT wide = _viv_image_wide, high = _viv_image_high;
    _viv_mipmap_t *mip;
    _viv_preview_t *preview;
    unsigned slot;
    if (!config_cache_last || _viv_frame_count != 1 || _viv_frame_loaded_count != 1 ||
        !*_viv_frame_fd->cFileName || !_viv_frames) return;

    mip = _viv_frames[0].mipmap;
    while (mip)
    {
        wide = wide / 2 + (wide & 1);
        high = high / 2 + (high & 1);
        if ((UINT64)wide * high <= FYTO_NEIGHBOR_PIXELS) break;
        mip = mip->mipmap;
    }
    if (!mip || !mip->hbitmap) return;

    slot = _viv_recent_preview_slot;
    _viv_recent_preview_slot = (slot + 1) % ARRAYSIZE(_viv_recent_previews);
    preview = &_viv_recent_previews[slot].image;
    if (preview->hbitmap) DeleteObject(preview->hbitmap);
    *preview = (_viv_preview_t){mip->hbitmap, wide, high, _viv_image_wide, _viv_image_high};
    mip->hbitmap = NULL;
    _viv_recent_previews[slot].file = *_viv_frame_fd;
}

'''+s[j:]
swap('\t\t_viv_keep_last_preview();\n','')
swap('''			_viv_frame_fd->cFileName[0] = 0;
		}
	}
}

static void _viv_activate_last''',
'''			_viv_frame_fd->cFileName[0] = 0;
		}
        else
            _viv_keep_current_preview();
	}
}

static void _viv_activate_last''')
swap('''		if (_viv_frame_count == _viv_frame_loaded_count)
		{
debug_printf("*** Cache LAST2''',
'''		if (_viv_frame_count == _viv_frame_loaded_count &&
            _viv_cache_image_fits(_viv_image_wide, _viv_image_high, _viv_frame_count))
		{
debug_printf("*** Cache LAST2''')
swap('''			_viv_frame_fd->cFileName[0] = 0;
		}
	}
	
	os_copy_memory(_viv_current_fd,_viv_last_fd''',
'''			_viv_frame_fd->cFileName[0] = 0;
		}
        else
            _viv_keep_current_preview();
	}
	
	os_copy_memory(_viv_current_fd,_viv_last_fd''')
swap('''    if (_viv_load_image_thread)
        SetThreadPriority(_viv_load_image_thread, THREAD_PRIORITY_NORMAL);
	os_copy_memory(_viv_current_fd,_viv_preload_fd,sizeof(WIN32_FIND_DATA));''',
'''    if (_viv_load_image_thread)
    {
        InterlockedExchange(&_viv_active_load->promoted, 1);
        SetThreadPriority(_viv_load_image_thread, THREAD_PRIORITY_NORMAL);
    }
	os_copy_memory(_viv_current_fd,_viv_preload_fd,sizeof(WIN32_FIND_DATA));
    if (!_viv_active_load && _viv_preload_preview.hbitmap && !_viv_preload_frames)
    {
        WIN32_FIND_DATA file = *_viv_preload_fd;
        _viv_load_is_preload = 0;
        _viv_show_preview(&_viv_preload_preview, &file);
        _viv_clear_preload();
        _viv_open(&file, 0);
        return;
    }''')
p.write_bytes((s.replace('\n','\r\n') if crlf else s).encode('utf-8'))
p=root/'voidImageViewer/src/fast_jpeg.h'
s=p.read_bytes()
a=b'    if ((UINT64)jpeg->wide * jpeg->high < 8000000 || !image_edge ||'
assert s.count(a)==1
p.write_bytes(s.replace(a,b'    if (!image_edge ||'))
