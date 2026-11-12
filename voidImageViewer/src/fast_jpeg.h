/* Windows JPEG decoding for fyto. See LICENSE. */
#ifndef FYTO_FAST_JPEG_H
#define FYTO_FAST_JPEG_H

/* Use the inbox JPEG codec explicitly, even when other WIC codecs are installed. */
#define COBJMACROS
#ifdef _MSC_VER
#pragma comment(lib, "windowscodecs.lib")
#else
#include <initguid.h>
#endif
#include <wincodec.h>

typedef struct fyto_wic_image {
    IWICImagingFactory *factory;
    IWICBitmapDecoder *decoder;
    IWICBitmapFrameDecode *frame;
    IWICColorContext *profile, *srgb;
    UINT wide, high;
} fyto_wic_image;

static HBITMAP fyto_image_dib(UINT wide, UINT high, UINT bit_count, BYTE **pixels)
{
    BITMAPINFO info = {0};
    BYTE *unused;
    UINT64 stride = (((UINT64)wide * bit_count + 31) / 32) * 4;
    if (!wide || !high || wide > INT_MAX || high > INT_MAX ||
        stride > UINT_MAX / high) return NULL;
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = wide;
    info.bmiHeader.biHeight = -(LONG)high;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = (WORD)bit_count;
    info.bmiHeader.biCompression = BI_RGB;
    return CreateDIBSection(NULL, &info, DIB_RGB_COLORS, (void **)(pixels ? pixels : &unused), NULL, 0);
}

static void fyto_wic_close(fyto_wic_image *jpeg)
{
    if (jpeg->srgb) IWICColorContext_Release(jpeg->srgb);
    if (jpeg->profile) IWICColorContext_Release(jpeg->profile);
    if (jpeg->frame) IWICBitmapFrameDecode_Release(jpeg->frame);
    if (jpeg->decoder) IWICBitmapDecoder_Release(jpeg->decoder);
    if (jpeg->factory) IWICImagingFactory_Release(jpeg->factory);
}

static BOOL fyto_wic_set_color(fyto_wic_image *jpeg)
{
    UINT count = 0;
    if (FAILED(IWICBitmapFrameDecode_GetColorContexts(jpeg->frame, 0, NULL, &count))) return FALSE;
    if (!count) return TRUE;
    return count == 1 &&
        SUCCEEDED(IWICImagingFactory_CreateColorContext(jpeg->factory, &jpeg->profile)) &&
        SUCCEEDED(IWICBitmapFrameDecode_GetColorContexts(jpeg->frame, 1, &jpeg->profile, &count)) &&
        count == 1 &&
        SUCCEEDED(IWICImagingFactory_CreateColorContext(jpeg->factory, &jpeg->srgb)) &&
        SUCCEEDED(IWICColorContext_InitializeFromExifColorSpace(jpeg->srgb, 1));
}

static BOOL fyto_wic_open(fyto_wic_image *image, const CLSID *codec, IStream *stream, WICPixelFormatGUID *format)
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

static int fyto_jpeg_orientation(const fyto_wic_image *jpeg)
{
    IWICMetadataQueryReader *reader;
    PROPVARIANT value;
    int orientation = 0;
    if (FAILED(IWICBitmapFrameDecode_GetMetadataQueryReader(jpeg->frame, &reader))) return -1;

    PropVariantInit(&value);
    if (SUCCEEDED(IWICMetadataQueryReader_GetMetadataByName(reader, L"/app1/ifd/{ushort=274}", &value)) &&
        value.vt == VT_UI2 && value.uiVal >= 1 && value.uiVal <= 8)
        orientation = value.uiVal;
    PropVariantClear(&value);
    if (!orientation)
    {
        if (SUCCEEDED(IWICMetadataQueryReader_GetMetadataByName(reader, L"/xmp/tiff:Orientation", &value)) &&
            value.vt == VT_LPWSTR && value.pwszVal &&
            value.pwszVal[0] >= L'1' && value.pwszVal[0] <= L'8' && !value.pwszVal[1])
            orientation = value.pwszVal[0] - L'0';
        PropVariantClear(&value);
    }
    IWICMetadataQueryReader_Release(reader);
    return orientation;
}

static BOOL fyto_wic_color_transform(const fyto_wic_image *jpeg, IWICBitmapSource *source, const WICPixelFormatGUID *format, IWICColorTransform **transform)
{
    return SUCCEEDED(IWICImagingFactory_CreateColorTransformer(jpeg->factory, transform)) &&
        SUCCEEDED(IWICColorTransform_Initialize(*transform, source, jpeg->profile, jpeg->srgb,
                                                format));
}

static WICBitmapTransformOptions fyto_wic_orientation(int orientation)
{
    switch (orientation)
    {
        case 2: return WICBitmapTransformFlipHorizontal;
        case 3: return WICBitmapTransformRotate180;
        case 4: return WICBitmapTransformFlipVertical;
        case 5: return WICBitmapTransformRotate90 | WICBitmapTransformFlipVertical;
        case 6: return WICBitmapTransformRotate90;
        case 7: return WICBitmapTransformRotate90 | WICBitmapTransformFlipHorizontal;
        case 8: return WICBitmapTransformRotate270;
        default: return WICBitmapTransformRotate0;
    }
}

static HBITMAP fyto_jpeg_full(const fyto_wic_image *jpeg, int orientation, UINT *wide, UINT *high)
{
    IWICFormatConverter *converter = NULL;
    IWICColorTransform *color = NULL;
    IWICBitmapFlipRotator *rotator = NULL;
    IWICBitmapSource *source;
    WICBitmapTransformOptions transform = fyto_wic_orientation(orientation);
    BYTE *pixels;
    HBITMAP bitmap = NULL;

    source = (IWICBitmapSource *)jpeg->frame;
    if (jpeg->profile)
    {
        if (!fyto_wic_color_transform(jpeg, source, &GUID_WICPixelFormat24bppBGR, &color)) goto done;
        source = (IWICBitmapSource *)color;
    }
    if (FAILED(IWICImagingFactory_CreateFormatConverter(jpeg->factory, &converter))) goto done;
    if (FAILED(IWICFormatConverter_Initialize(converter, source,
        &GUID_WICPixelFormat32bppBGR, WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom))) goto done;
    source = (IWICBitmapSource *)converter;
    if (transform != WICBitmapTransformRotate0)
    {
        if (FAILED(IWICImagingFactory_CreateBitmapFlipRotator(jpeg->factory, &rotator)) ||
            FAILED(IWICBitmapFlipRotator_Initialize(rotator, source, transform))) goto done;
        source = (IWICBitmapSource *)rotator;
    }
    if (FAILED(IWICBitmapSource_GetSize(source, wide, high))) goto done;
    bitmap = fyto_image_dib(*wide, *high, 32, &pixels);
    if (bitmap && FAILED(IWICBitmapSource_CopyPixels(source, NULL, *wide * 4, *wide * 4 * *high, pixels)))
    {
        DeleteObject(bitmap);
        bitmap = NULL;
    }

done:
    if (rotator) IWICBitmapFlipRotator_Release(rotator);
    if (converter) IWICFormatConverter_Release(converter);
    if (color) IWICColorTransform_Release(color);
    return bitmap;
}

static HBITMAP fyto_jpeg_color_preview(const fyto_wic_image *jpeg, BYTE *pixels, UINT wide, UINT high, UINT stride)
{
    IWICBitmap *source = NULL;
    IWICColorTransform *color = NULL;
    HBITMAP bitmap = NULL;
    BYTE *converted;
    if (SUCCEEDED(IWICImagingFactory_CreateBitmapFromMemory(jpeg->factory, wide, high,
        &GUID_WICPixelFormat24bppBGR, stride, stride * high, pixels, &source)) &&
        fyto_wic_color_transform(jpeg, (IWICBitmapSource *)source, &GUID_WICPixelFormat24bppBGR, &color))
    {
        bitmap = fyto_image_dib(wide, high, 24, &converted);
        if (bitmap && FAILED(IWICColorTransform_CopyPixels(color, NULL, stride, stride * high, converted)))
        {
            DeleteObject(bitmap);
            bitmap = NULL;
        }
    }
    if (color) IWICColorTransform_Release(color);
    if (source) IWICBitmap_Release(source);
    return bitmap;
}

static HBITMAP fyto_jpeg_preview(const fyto_wic_image *jpeg, UINT edge, UINT *wide, UINT *high)
{
    IWICBitmapSourceTransform *transform;
    WICPixelFormatGUID format = GUID_WICPixelFormat24bppBGR;
    HBITMAP bitmap = NULL;
    BYTE *pixels;
    UINT stride, image_edge = max(jpeg->wide, jpeg->high);

    if (!image_edge ||
        FAILED(IWICBitmapFrameDecode_QueryInterface(jpeg->frame, &IID_IWICBitmapSourceTransform, (void **)&transform)))
        return NULL;

    /* The system JPEG decoder can decode at reduced resolution directly. */
    edge = min(edge, (image_edge + 3) / 4);
    *wide = max(1, (UINT)((UINT64)edge * jpeg->wide / image_edge));
    *high = max(1, (UINT)((UINT64)edge * jpeg->high / image_edge));
    if (SUCCEEDED(IWICBitmapSourceTransform_GetClosestSize(transform, wide, high)) &&
        *wide < jpeg->wide && *high < jpeg->high &&
        SUCCEEDED(IWICBitmapSourceTransform_GetClosestPixelFormat(transform, &format)) &&
        IsEqualGUID(&format, &GUID_WICPixelFormat24bppBGR))
    {
        bitmap = fyto_image_dib(*wide, *high, 24, &pixels);
        if (bitmap)
        {
            stride = (*wide * 3 + 3) & ~3u;
            if (FAILED(IWICBitmapSourceTransform_CopyPixels(transform, NULL, *wide, *high, &format,
                WICBitmapTransformRotate0, stride, stride * *high, pixels)))
            {
                DeleteObject(bitmap);
                bitmap = NULL;
            }
            else if (jpeg->profile)
            {
                HBITMAP colored = fyto_jpeg_color_preview(jpeg, pixels, *wide, *high, stride);
                DeleteObject(bitmap);
                bitmap = colored;
            }
        }
    }
    IWICBitmapSourceTransform_Release(transform);
    return bitmap;
}

#endif

