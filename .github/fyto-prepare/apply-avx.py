from pathlib import Path
import sys
root=Path(sys.argv[1])
src=root/'voidImageViewer/src'
def write(p,s):
    crlf=b'\r\n' in p.read_bytes() if p.exists() else False
    p.write_bytes(s.replace('\n','\r\n' if crlf else '\n').encode('utf-8'))

png=src/'fast_png.h'
s=png.read_text(encoding='utf-8')
i=s.index('#define WUFFS_IMPLEMENTATION')
j=s.index('static HBITMAP fyto_png_full',i)
module=s[i:s.index('static HBITMAP fyto_png_wuffs',i)]
c = '''/* The AVX instructions stay in this translation unit. */
#include <windows.h>

''' + module + '''BOOL fyto_decode_png_avx(BYTE *data, DWORD size, BYTE *pixels, UINT wide, UINT high, BOOL alpha,
    BOOL (*cancelled)(void *), void *context)
{
    wuffs_png__decoder *decoder;
    wuffs_base__io_buffer input;
    wuffs_base__image_config config;
    wuffs_base__pixel_buffer output;
    wuffs_base__status status;
    uint64_t work_size;
    BYTE *work = NULL;
    BOOL decoded = FALSE;

    decoder = wuffs_png__decoder__alloc();
    if (!decoder) return FALSE;
    input = wuffs_base__ptr_u8__reader(data, size, true);
    status = wuffs_png__decoder__decode_image_config(decoder, &config, &input);
    if (status.repr || cancelled(context) ||
        wuffs_base__pixel_config__width(&config.pixcfg) != wide ||
        wuffs_base__pixel_config__height(&config.pixcfg) != high) goto done;
    wuffs_base__pixel_config__set(&config.pixcfg,
        alpha ? WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL : WUFFS_BASE__PIXEL_FORMAT__BGRX,
        WUFFS_BASE__PIXEL_SUBSAMPLING__NONE, wide, high);
    status = wuffs_base__pixel_buffer__set_from_slice(&output, &config.pixcfg,
        wuffs_base__make_slice_u8(pixels, (size_t)wide * high * 4));
    if (status.repr) goto done;
    work_size = wuffs_png__decoder__workbuf_len(decoder).max_incl;
    if (work_size > SIZE_MAX) goto done;
    work = malloc((size_t)work_size);
    if (!work) goto done;
    input.meta.wi = input.meta.ri;
    do
    {
        if (cancelled(context)) goto done;
        input.meta.wi += min((size_t)size - input.meta.wi, 256 * 1024);
        input.meta.closed = input.meta.wi == size;
        status = wuffs_png__decoder__decode_frame(decoder, &output, &input, WUFFS_BASE__PIXEL_BLEND__SRC,
            wuffs_base__make_slice_u8(work, (size_t)work_size), NULL);
    }
    while (status.repr == wuffs_base__suspension__short_read && !input.meta.closed);
    decoded = !status.repr && !cancelled(context);

done:
    free(work);
    free(decoder);
    return decoded;
}
'''
write(src/'png_decode_avx.c',c)
s=s[:i]+'''#ifdef FYTO_HAVE_AVX_PNG
BOOL fyto_decode_png_avx(BYTE *data, DWORD size, BYTE *pixels, UINT wide, UINT high, BOOL alpha,
    BOOL (*cancelled)(void *), void *context);

static HBITMAP fyto_png_wuffs(IStream *stream, DWORD size, BOOL alpha,
    UINT *wide, UINT *high, BOOL (*cancelled)(void *), void *context)
{
    HGLOBAL storage;
    BYTE *data, *pixels;
    HBITMAP bitmap;
    UINT width, height;
    if (!IsProcessorFeaturePresent(PF_AVX_INSTRUCTIONS_AVAILABLE) ||
        FAILED(GetHGlobalFromStream(stream, &storage))) return NULL;
    data = GlobalLock(storage);
    if (!data) return NULL;
    width = fyto_png_u32(data + 16);
    height = fyto_png_u32(data + 20);
    bitmap = fyto_image_dib(width, height, 32, &pixels);
    if (bitmap && !fyto_decode_png_avx(data, size, pixels, width, height, alpha, cancelled, context))
    {
        DeleteObject(bitmap);
        bitmap = NULL;
    }
    GlobalUnlock(storage);
    if (bitmap)
    {
        if (alpha) fyto_png_composite(pixels, width, height);
        *wide = width;
        *high = height;
    }
    return bitmap;
}
#endif

'''+s[j:]
write(png,s)
p=src/'viv.c'
s=p.read_text(encoding='utf-8')
s=s.replace('    DWORD file_size = 0;', '#ifdef FYTO_HAVE_AVX_PNG\n    DWORD file_size = 0;\n#endif')
s=s.replace('                        file_size = size;', '#ifdef FYTO_HAVE_AVX_PNG\n                        file_size = size;\n#endif')
s=s.replace('''            if (!(png_flags & FYTO_PNG_ICC))
                first_frame.frame.hbitmap = fyto_png_wuffs(stream, file_size, alpha,
                    &first_frame.wide, &first_frame.high, _viv_load_discarded, job);''',
'''#ifdef FYTO_HAVE_AVX_PNG
            if (!(png_flags & FYTO_PNG_ICC))
                first_frame.frame.hbitmap = fyto_png_wuffs(stream, file_size, alpha,
                    &first_frame.wide, &first_frame.high, _viv_load_discarded, job);
#endif''')
s=s.replace('''    PostMessageW(scan->hwnd, FYTO_DIRECTORY_READY, 0, 0);''',
'''    if (!scan->ready)
    {
        free(scan->files);
        scan->files = NULL;
        scan->count = 0;
    }
    PostMessageW(scan->hwnd, FYTO_DIRECTORY_READY, 0, 0);''')
directory_shutdown='''    if (_viv_directory_scan)
    {
        InterlockedExchange(&_viv_directory_scan->cancelled, 1);
        WaitForSingleObject(_viv_directory_scan->thread, INFINITE);
        _viv_free_directory(_viv_directory_scan);
    }
    _viv_free_directory(_viv_directory_cache);
'''
assert s.count(directory_shutdown)==1
s=s.replace(directory_shutdown,'')
kill=s.index('static void _viv_kill(void)\n{')
e=s.index('\tif (_viv_last_frames)',kill)
s=s[:e]+directory_shutdown+'\n'+s[e:]
write(p,s)
proj=root/'voidImageViewer/vs2019/voidImageViewer.vcxproj'
s=proj.read_text(encoding='utf-8-sig')
old='WIN32;NDEBUG;_WINDOWS;BZ_NO_STDIO;VERSION_X64;%(PreprocessorDefinitions)'
assert s.count(old)==1
s=s.replace(old,old.replace('VERSION_X64;','VERSION_X64;FYTO_HAVE_AVX_PNG;'))
old='    <ClCompile Include="..\\src\\viv.c" />'
assert s.count(old)==1
s=s.replace(old,old+'''
    <ClCompile Include="..\\src\\png_decode_avx.c" Condition="'$(Configuration)|$(Platform)'=='Release|x64'">
      <AdditionalOptions>/arch:AVX %(AdditionalOptions)</AdditionalOptions>
      <WholeProgramOptimization>false</WholeProgramOptimization>
    </ClCompile>''')
bom=b'\xef\xbb\xbf' if proj.read_bytes().startswith(b'\xef\xbb\xbf') else b''
write(proj,s)
if bom:proj.write_bytes(bom+proj.read_bytes())
p=root/'scripts/Build-Release.ps1'
s=p.read_text(encoding='utf-8')
old="Copy-Item (Join-Path $root 'THIRD_PARTY_NOTICES.md') (Join-Path $payload 'NOTICE.txt')"
assert s.count(old)==1
s=s.replace(old,"Copy-Item (Join-Path $root 'voidImageViewer/wuffs/LICENSE') (Join-Path $payload 'wuffs-LICENSE')\n"+old)
write(p,s)
p=root/'voidImageViewer/installer/Package.wxs'
s=p.read_text(encoding='utf-8')
old='<File Source="$(var.Payload)\\NOTICE.txt" />'
assert s.count(old)==1
s=s.replace(old,old+'\n          <File Source="$(var.Payload)\\wuffs-LICENSE" />')
write(p,s)
p=root/'THIRD_PARTY_NOTICES.md'
s=p.read_text(encoding='utf-8')
preamble='''The bundled Wuffs PNG decoder is from Wuffs v0.3.4, commit ec71f9c6d829ca763fbbc1f7adecc30a89a8ed0a. Copyright 2017 The Wuffs Authors. The unmodified source and Apache 2.0 license are in voidImageViewer/wuffs. Binary distributions include the license as wuffs-LICENSE. Upstream source is at https://github.com/google/wuffs.

'''
s=s.replace('The installer includes',preamble+'The installer includes')
write(p,s)
