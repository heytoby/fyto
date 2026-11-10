from pathlib import Path
import sys
root = Path(sys.argv[1]) / 'voidImageViewer' / 'src'
def save(path, text):
    path.write_bytes(text.replace('\n','\r\n').encode('utf-8'))
def swap(text, before, after):
    assert text.count(before) == 1, before
    return text.replace(before, after)
jpeg = root / 'fast_jpeg.h'
s = jpeg.read_text(encoding='utf-8')
for a,b in [('struct fyto_jpeg','struct fyto_wic_image'),('} fyto_jpeg;','} fyto_wic_image;'),('fyto_jpeg *','fyto_wic_image *'),('fyto_jpeg_close','fyto_wic_close'),('fyto_jpeg_set_color','fyto_wic_set_color')]:
    s = s.replace(a,b)
i = s.index('static BOOL fyto_jpeg_open')
j = s.index('static int fyto_jpeg_orientation', i)
s = s[:i] + '''static BOOL fyto_wic_open(fyto_wic_image *image, const CLSID *codec, IStream *stream, WICPixelFormatGUID *format)
{
    return SUCCEEDED(CoCreateInstance(codec, NULL, CLSCTX_INPROC_SERVER,
                                     &IID_IWICBitmapDecoder, (void **)&image->decoder)) &&
        SUCCEEDED(IWICBitmapDecoder_Initialize(image->decoder, stream, WICDecodeMetadataCacheOnDemand)) &&
        SUCCEEDED(IWICBitmapDecoder_GetFrame(image->decoder, 0, &image->frame)) &&
        SUCCEEDED(IWICBitmapFrameDecode_GetSize(image->frame, &image->wide, &image->high)) &&
        SUCCEEDED(IWICBitmapFrameDecode_GetPixelFormat(image->frame, format)) &&
        SUCCEEDED(CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                   &IID_IWICImagingFactory, (void **)&image->factory));
}

static BOOL fyto_jpeg_open(fyto_wic_image *jpeg, IStream *stream, BOOL use_icm)
{
    WICPixelFormatGUID format;
    return fyto_wic_open(jpeg, &CLSID_WICJpegDecoder, stream, &format) &&
        (IsEqualGUID(&format, &GUID_WICPixelFormat24bppBGR) ||
         IsEqualGUID(&format, &GUID_WICPixelFormat32bppBGR) ||
         IsEqualGUID(&format, &GUID_WICPixelFormat8bppGray)) &&
        (!use_icm || fyto_wic_set_color(jpeg));
}

''' + s[j:]
s = swap(s,
    'static BOOL fyto_jpeg_color_transform(const fyto_wic_image *jpeg, IWICBitmapSource *source, IWICColorTransform **transform)',
    'static BOOL fyto_wic_color_transform(const fyto_wic_image *jpeg, IWICBitmapSource *source, const WICPixelFormatGUID *format, IWICColorTransform **transform)')
s = swap(s, '&GUID_WICPixelFormat24bppBGR));\n}', 'format));\n}')
s = s.replace('fyto_jpeg_color_transform(jpeg, source, &color)', 'fyto_wic_color_transform(jpeg, source, &GUID_WICPixelFormat24bppBGR, &color)')
s = s.replace('fyto_jpeg_color_transform(jpeg, (IWICBitmapSource *)source, &color)', 'fyto_wic_color_transform(jpeg, (IWICBitmapSource *)source, &GUID_WICPixelFormat24bppBGR, &color)')
save(jpeg,s)
viv = root / 'viv.c'
s = viv.read_text(encoding='utf-8')
s = swap(s,'#include "fast_jpeg.h"','#include "fast_jpeg.h"\n#include "fast_png.h"')
s = swap(s,'    fyto_jpeg jpeg = {0};','    fyto_wic_image jpeg = {0};')
s = swap(s,'    fyto_jpeg_close(&jpeg);','    fyto_wic_close(&jpeg);')
start = s.index('static BOOL _viv_load_fast_jpeg(IStream *stream,')
s = s[:start] + '''static void _viv_reply_still_frame(_viv_reply_load_image_first_frame_t *frame)
{
    int mip_wide, mip_high;
    frame->frame_count = 1;
    _viv_get_mipmap(frame->frame.hbitmap, frame->wide, frame->high,
        _viv_load_render_wide, _viv_load_render_high, &mip_wide, &mip_high, &frame->frame.mipmap);
    _viv_reply_add(_VIV_REPLY_LOAD_IMAGE_FIRST_FRAME, sizeof(*frame), frame);
}

''' + s[start:]
s = swap(s, '''                int mip_wide, mip_high;
                frame->frame_count = 1;
                _viv_get_mipmap(frame->frame.hbitmap, frame->wide, frame->high,
                    _viv_load_render_wide, _viv_load_render_high, &mip_wide, &mip_high, &frame->frame.mipmap);
                _viv_reply_add(_VIV_REPLY_LOAD_IMAGE_FIRST_FRAME, sizeof(*frame), frame);''',
    '                _viv_reply_still_frame(frame);')
s = swap(s, 'BOOL jpeg_file = FALSE;\n\tDWORD tickstart;', 'BOOL jpeg_file = FALSE;\n    int png_alpha = -1;\n\tDWORD tickstart;')
s = swap(s, '(BYTE)buf[2] == 0xff;\n\t\t\t\t\tGlobalUnlock', '''(BYTE)buf[2] == 0xff;
                    if (totreadsize == 0 && !jpeg_file)
                        png_alpha = fyto_png_alpha((const BYTE *)buf, size, config_orientation, config_icm);
\t\t\t\t\tGlobalUnlock''')
s = swap(s, '''        if (jpeg_file && _viv_load_fast_jpeg(stream, &orientation, &first_frame))
            ret = 1;
        else if (!_viv_load_image_terminate || _viv_load_image_allow_draw)''', '''        if (jpeg_file && _viv_load_fast_jpeg(stream, &orientation, &first_frame))
            ret = 1;
        if (!ret && png_alpha >= 0 && (!_viv_load_image_terminate || _viv_load_image_allow_draw))
        {
            first_frame.frame.hbitmap = fyto_png_full(stream, png_alpha, config_icm,
                &first_frame.wide, &first_frame.high);
            if (first_frame.frame.hbitmap)
            {
                _viv_reply_still_frame(&first_frame);
                ret = 1;
            }
        }
        if (!ret && (!_viv_load_image_terminate || _viv_load_image_allow_draw))''')
s = swap(s,'            if (jpeg_file)\n            {\n                LARGE_INTEGER start = {0};',
    '            if (jpeg_file || png_alpha >= 0)\n            {\n                LARGE_INTEGER start = {0};')
save(viv,s)
