#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include "fyto_text_render.h"

template<class T> static void release(T *&value)
{
    if (value)value->Release();
    value=nullptr;
}

static IDWriteFactory *write_factory;
static IDWriteRenderingParams *rendering_params;
static ID2D1Factory *draw_factory;
static ID2D1DCRenderTarget *draw_target;
static ID2D1SolidColorBrush *ink;
static HDC layer;
static HBITMAP layer_bitmap;
static SIZE layer_size;
static const wchar_t font_family[]=L"Segoe UI";

static void release_target()
{
    release(ink);
    release(draw_target);
}

void fyto_text_shutdown()
{
    release_target();
    if (layer)DeleteDC(layer);
    if (layer_bitmap)DeleteObject(layer_bitmap);
    layer=nullptr;
    layer_bitmap=nullptr;
    layer_size={};
    release(draw_factory);
    release(rendering_params);
    release(write_factory);
}

BOOL fyto_text_initialize()
{
    if (write_factory)return TRUE;
    HRESULT hr=DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown **>(&write_factory));
    if (SUCCEEDED(hr))
        hr=write_factory->CreateCustomRenderingParams(1.8f,0.6f,0.0f,DWRITE_PIXEL_GEOMETRY_FLAT,
            DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC,&rendering_params);
    if (SUCCEEDED(hr))hr=D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&draw_factory);
    if (FAILED(hr))fyto_text_shutdown();
    return SUCCEEDED(hr);
}

static HRESULT make_target()
{
    if (draw_target)return S_OK;
    auto properties=D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED),96.0f,96.0f);
    HRESULT hr=draw_factory->CreateDCRenderTarget(&properties,&draw_target);
    if (SUCCEEDED(hr))hr=draw_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White),&ink);
    if (FAILED(hr))release_target();
    return hr;
}

static HRESULT make_layer(HDC dc,LONG width,LONG height)
{
    if (layer_bitmap && width<=layer_size.cx && height<=layer_size.cy)return S_OK;
    if (!layer)layer=CreateCompatibleDC(dc);
    if (!layer)return E_OUTOFMEMORY;

    BITMAPINFO info={};
    info.bmiHeader.biSize=sizeof(info.bmiHeader);
    info.bmiHeader.biWidth=max(width,layer_size.cx);
    info.bmiHeader.biHeight=-max(height,layer_size.cy);
    info.bmiHeader.biPlanes=1;
    info.bmiHeader.biBitCount=32;
    void *bits;
    HBITMAP bitmap=CreateDIBSection(layer,&info,DIB_RGB_COLORS,&bits,nullptr,0);
    if (!bitmap)return E_OUTOFMEMORY;
    if (!SelectObject(layer,bitmap))
    {
        DeleteObject(bitmap);
        return E_FAIL;
    }
    if (layer_bitmap)DeleteObject(layer_bitmap);
    layer_bitmap=bitmap;
    layer_size={info.bmiHeader.biWidth,-info.bmiHeader.biHeight};
    return S_OK;
}

static HRESULT make_layout(const wchar_t *text,UINT32 length,UINT flags,int pixels,
    float width,float height,IDWriteTextLayout **layout)
{
    IDWriteTextFormat *format=nullptr;
    IDWriteInlineObject *ellipsis=nullptr;
    wchar_t label[256];
    UINT32 mnemonic=length;
    HRESULT hr;

    if (!(flags&DT_NOPREFIX))
    {
        UINT32 out=0;
        if (length>=ARRAYSIZE(label))return E_INVALIDARG;
        for (UINT32 i=0; i<length; i++)
        {
            if (text[i]==L'&')
            {
                if (++i==length)break;
                if (text[i]!=L'&')mnemonic=out;
            }
            label[out++]=text[i];
        }
        text=label;
        length=out;
    }
    hr=write_factory->CreateTextFormat(font_family,nullptr,DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,static_cast<float>(pixels),L"zh-CN",&format);
    if (SUCCEEDED(hr))hr=format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    if (SUCCEEDED(hr) && flags&DT_VCENTER)
        hr=format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    if (SUCCEEDED(hr) && flags&(DT_RIGHT|DT_CENTER))
        hr=format->SetTextAlignment(flags&DT_RIGHT?DWRITE_TEXT_ALIGNMENT_TRAILING:DWRITE_TEXT_ALIGNMENT_CENTER);
    if (SUCCEEDED(hr) && flags&DT_END_ELLIPSIS)
    {
        DWRITE_TRIMMING trimming={DWRITE_TRIMMING_GRANULARITY_CHARACTER,0,0};
        hr=write_factory->CreateEllipsisTrimmingSign(format,&ellipsis);
        if (SUCCEEDED(hr))hr=format->SetTrimming(&trimming,ellipsis);
    }
    if (SUCCEEDED(hr))hr=write_factory->CreateTextLayout(text,length,format,width,height,layout);
    if (SUCCEEDED(hr) && mnemonic<length && !(flags&DT_HIDEPREFIX))
        hr=(*layout)->SetUnderline(TRUE,DWRITE_TEXT_RANGE{mnemonic,1});
    release(ellipsis);
    release(format);
    return hr;
}

BOOL fyto_text_render(HDC dc,const wchar_t *text,int length,RECT *rect,UINT flags,int pixels)
{
    IDWriteTextLayout *layout=nullptr;
    BOOL measure=(flags&DT_CALCRECT)!=0;
    HRESULT hr;

    if (!write_factory || !text || pixels<=0)return FALSE;
    if (!measure && IsRectEmpty(rect))return TRUE;
    if (length<0)length=lstrlenW(text);
    hr=make_layout(text,static_cast<UINT32>(length),flags,pixels,
        measure?100000.0f:static_cast<float>(rect->right-rect->left),
        measure?4096.0f:static_cast<float>(rect->bottom-rect->top),&layout);
    if (SUCCEEDED(hr) && measure)
    {
        DWRITE_TEXT_METRICS metrics;
        hr=layout->GetMetrics(&metrics);
        if (SUCCEEDED(hr))
        {
            rect->right=rect->left+static_cast<LONG>(metrics.widthIncludingTrailingWhitespace+0.999f);
            rect->bottom=rect->top+static_cast<LONG>(metrics.height+0.999f);
        }
    }
    if (SUCCEEDED(hr) && !measure)
    {
        RECT bounds={0,0,rect->right-rect->left,rect->bottom-rect->top};
        hr=make_target();
        if (SUCCEEDED(hr))hr=make_layer(dc,bounds.right,bounds.bottom);
        if (SUCCEEDED(hr))hr=draw_target->BindDC(layer,&bounds);
        if (SUCCEEDED(hr))
        {
            COLORREF color=GetTextColor(dc);
            draw_target->BeginDraw();
            draw_target->Clear(D2D1::ColorF(0,0,0,0));
            draw_target->SetTextRenderingParams(rendering_params);
            draw_target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
            ink->SetColor(D2D1::ColorF(GetRValue(color)/255.0f,GetGValue(color)/255.0f,GetBValue(color)/255.0f));
            draw_target->DrawTextLayout(D2D1::Point2F(),layout,ink,D2D1_DRAW_TEXT_OPTIONS_CLIP);
            hr=draw_target->EndDraw();
            if (hr==D2DERR_RECREATE_TARGET)release_target();
            if (SUCCEEDED(hr))
            {
                BLENDFUNCTION blend={AC_SRC_OVER,0,255,AC_SRC_ALPHA};
                if (!AlphaBlend(dc,rect->left,rect->top,bounds.right,bounds.bottom,
                    layer,0,0,bounds.right,bounds.bottom,blend))hr=E_FAIL;
            }
        }
    }
    release(layout);
    return SUCCEEDED(hr);
}
