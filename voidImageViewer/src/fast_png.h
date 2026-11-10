/* Common static PNGs can go directly from WIC into the viewer's bitmap. */
#ifndef FYTO_FAST_PNG_H
#define FYTO_FAST_PNG_H

static UINT fyto_png_u32(const BYTE *p)
{
    return ((UINT)p[0] << 24) | ((UINT)p[1] << 16) | ((UINT)p[2] << 8) | p[3];
}

enum { FYTO_PNG_ALPHA = 1, FYTO_PNG_ICC = 2 };

static int fyto_png_flags(const BYTE *data, DWORD size, BOOL orientation, BOOL icm)
{
    static const BYTE signature[] = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    BOOL alpha, gamma = FALSE, srgb = FALSE, profile = FALSE;
    if (size < 33 || memcmp(data, signature, sizeof(signature)) ||
        fyto_png_u32(data + 8) != 13 || memcmp(data + 12, "IHDR", 4) ||
        data[24] != 8 || (data[25] != 2 && data[25] != 6))
        return -1;
    alpha = data[25] == 6;
    data += 33;
    size -= 33;
    while (size >= 12)
    {
        UINT length = fyto_png_u32(data);
        const BYTE *kind = data + 4;
        if (length > size - 12) return -1;
        if (!memcmp(kind, "IEND", 4))
            return icm && gamma && !srgb && !profile ? -1 :
                (alpha ? FYTO_PNG_ALPHA : 0) | (icm && profile ? FYTO_PNG_ICC : 0);
        if (!memcmp(kind, "acTL", 4)) return -1;
        if (orientation && (!memcmp(kind, "eXIf", 4) ||
            (length >= 18 && (!memcmp(kind, "iTXt", 4) || !memcmp(kind, "tEXt", 4) ||
                              !memcmp(kind, "zTXt", 4)) && !memcmp(data + 8, "XML:com.adobe.xmp", 18))))
            return -1;
        if (!memcmp(kind, "tRNS", 4)) alpha = TRUE;
        if (!memcmp(kind, "iCCP", 4)) profile = TRUE;
        if (!memcmp(kind, "sRGB", 4)) srgb = TRUE;
        if (!memcmp(kind, "gAMA", 4) || !memcmp(kind, "cHRM", 4)) gamma = TRUE;
        data += length + 12;
        size -= length + 12;
    }
    return -1;
}

static void fyto_png_composite(BYTE *pixels, UINT wide, UINT high)
{
    UINT x, y;
    for (y = 0; y < high; ++y)
    {
        BYTE *pixel = pixels + (size_t)y * wide * 4;
        for (x = 0; x < wide; ++x, pixel += 4)
        {
            UINT a = pixel[3];
            if (a != 255)
            {
                UINT gray = ((x ^ y) & 16) ? 0xd8 : 0xf0;
                BYTE background = (BYTE)((gray * (255 - a) + 127) / 255);
                pixel[0] += background;
                pixel[1] += background;
                pixel[2] += background;
            }
        }
    }
}

#ifdef FYTO_HAVE_AVX_PNG
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

static HBITMAP fyto_png_full(IStream *stream, BOOL alpha, BOOL icm, UINT *wide, UINT *high)
{
    fyto_wic_image png = {0};
    IWICBitmapSource *source;
    IWICFormatConverter *converter = NULL;
    IWICColorTransform *color = NULL;
    WICPixelFormatGUID native_format;
    const WICPixelFormatGUID *format = alpha ? &GUID_WICPixelFormat32bppPBGRA : &GUID_WICPixelFormat32bppBGR;
    HBITMAP bitmap = NULL;
    BYTE *pixels;

    if (!fyto_wic_open(&png, &CLSID_WICPngDecoder, stream, &native_format) ||
        (icm && !fyto_wic_set_color(&png))) goto done;
    source = (IWICBitmapSource *)png.frame;
    if (png.profile)
    {
        const WICPixelFormatGUID *color_format = alpha ? &GUID_WICPixelFormat32bppBGRA : &GUID_WICPixelFormat24bppBGR;
        if (!fyto_wic_color_transform(&png, source, color_format, &color)) goto done;
        source = (IWICBitmapSource *)color;
        native_format = *color_format;
    }
    if (!IsEqualGUID(&native_format, format))
    {
        if (FAILED(IWICImagingFactory_CreateFormatConverter(png.factory, &converter)) ||
            FAILED(IWICFormatConverter_Initialize(converter, source, format,
                WICBitmapDitherTypeNone, NULL, 0, WICBitmapPaletteTypeCustom))) goto done;
        source = (IWICBitmapSource *)converter;
    }
    bitmap = fyto_image_dib(png.wide, png.high, 32, &pixels);
    if (!bitmap) goto done;
    if (FAILED(IWICBitmapSource_CopyPixels(source, NULL, png.wide * 4, png.wide * 4 * png.high, pixels)))
    {
        DeleteObject(bitmap);
        bitmap = NULL;
        goto done;
    }
    if (alpha) fyto_png_composite(pixels, png.wide, png.high);
    *wide = png.wide;
    *high = png.high;

done:
    if (converter) IWICFormatConverter_Release(converter);
    if (color) IWICColorTransform_Release(color);
    fyto_wic_close(&png);
    return bitmap;
}

#endif
