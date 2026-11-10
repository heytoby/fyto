from pathlib import Path
import sys
r = Path(sys.argv[1])
w = Path(sys.argv[2])
f = r / 'voidImageViewer/src/fast_png.h'
s = f.read_text(encoding='utf-8')
s = s.replace('static int fyto_png_alpha(', 'enum { FYTO_PNG_ALPHA = 1, FYTO_PNG_ICC = 2 };\n\nstatic int fyto_png_flags(')
s = s.replace('return icm && gamma && !srgb && !profile ? -1 : alpha;', 'return icm && gamma && !srgb && !profile ? -1 :\n                (alpha ? FYTO_PNG_ALPHA : 0) | (icm && profile ? FYTO_PNG_ICC : 0);')
p = s.index('static HBITMAP fyto_png_full(')
s = s[:p] + (w / 'wuffs-wrapper.txt').read_text(encoding='utf-8') + s[p:]
f.write_bytes(s.replace('\n','\r\n').encode('utf-8'))
f = r / 'voidImageViewer/src/viv.c'
s = f.read_text(encoding='utf-8')
s = s.replace('static void _viv_cancel_load(BOOL keep_first)\n{', '''static BOOL _viv_load_discarded(void *context)
{
    return _viv_load_stop_state(context) == _VIV_LOAD_DISCARD;
}

static void _viv_cancel_load(BOOL keep_first)
{''')
s = s.replace('    int png_alpha = -1;', '    int png_flags = -1;\n    DWORD file_size = 0;').replace('png_alpha', 'png_flags')
s = s.replace('png_flags = fyto_png_flags((const BYTE *)buf, size, job->use_orientation, job->use_icm);', '''{
                        png_flags = fyto_png_flags((const BYTE *)buf, size, job->use_orientation, job->use_icm);
                        file_size = size;
                    }''')
old = '''            first_frame.frame.hbitmap = fyto_png_full(stream, png_flags, job->use_icm,
                &first_frame.wide, &first_frame.high);'''
new = '''            BOOL alpha = (png_flags & FYTO_PNG_ALPHA) != 0;
            if (!(png_flags & FYTO_PNG_ICC))
                first_frame.frame.hbitmap = fyto_png_wuffs(stream, file_size, alpha,
                    &first_frame.wide, &first_frame.high, _viv_load_discarded, job);
            if (!first_frame.frame.hbitmap && !_viv_load_discarded(job))
                first_frame.frame.hbitmap = fyto_png_full(stream, alpha, job->use_icm,
                    &first_frame.wide, &first_frame.high);'''
assert s.count(old) == 1
s = s.replace(old,new)
f.write_bytes(s.replace('\n','\r\n').encode('utf-8'))
