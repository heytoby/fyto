#ifndef FYTO_HOVER_PRESENTATION_H
#define FYTO_HOVER_PRESENTATION_H
#include "caption_metrics.h"
typedef struct {RECT filename,play,resolution;} fyto_hover_layout_t;
static HFONT fyto_hover_font,fyto_hover_caption_font;
static int fyto_hover_font_dpi;
static void fyto_hover_layout(const RECT *image,const RECT *canvas,int dpi_x,int dpi_y,fyto_hover_layout_t *out)
{
    int pad=MulDiv(12,dpi_x,96),box=MulDiv(40,dpi_y,96),text_h=MulDiv(20,dpi_y,96);
    int width=image->right-image->left,height=image->bottom-image->top,play_left;
    if(pad<2)pad=2;
    if(box>height-pad*2)box=max(0,height-pad*2);
    if(box>width-pad*2)box=max(0,width-pad*2);
    play_left=image->left+(width-box)/2;
    SetRect(&out->play,play_left,image->bottom-pad-box,play_left+box,image->bottom-pad);
    fyto_caption_filename_rect(canvas,dpi_x,dpi_y,0,&out->filename);
    SetRect(&out->resolution,out->play.right+pad,image->bottom-pad-text_h,image->right-pad,image->bottom-pad);
    if(out->filename.bottom+pad>out->play.top)SetRectEmpty(&out->filename);
    if(out->resolution.left>=out->resolution.right)SetRectEmpty(&out->resolution);
}
static int fyto_hover_shadow_radius(int dpi_y)
{
    return max(2,MulDiv(3,dpi_y,96));
}

static void fyto_hover_text(HDC dc,const wchar_t *text,RECT r,UINT flags,BYTE alpha,int dpi_y)
{
    int radius=fyto_hover_shadow_radius(dpi_y), span=2*radius+1;
    int width=r.right-r.left+2*radius, height=r.bottom-r.top+2*radius;
    HDC layer;
    HBITMAP bitmap;
    HGDIOBJ old_bitmap, old_font;
    BITMAPINFO info={0};
    DWORD *pixels;
    BYTE *horizontal;
    RECT local;
    int x,y;
    BLENDFUNCTION blend={AC_SRC_OVER,0,255,AC_SRC_ALPHA};

    if (!alpha || IsRectEmpty(&r))return;
    layer=CreateCompatibleDC(dc);
    if (!layer)return;

    info.bmiHeader.biSize=sizeof(info.bmiHeader);
    info.bmiHeader.biWidth=width;
    info.bmiHeader.biHeight=-height;
    info.bmiHeader.biPlanes=1;
    info.bmiHeader.biBitCount=32;
    info.bmiHeader.biCompression=BI_RGB;
    bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,(void **)&pixels,NULL,0);
    horizontal=bitmap?HeapAlloc(GetProcessHeap(),0,(SIZE_T)width*height):NULL;
    if (!horizontal)
    {
        if (bitmap)DeleteObject(bitmap);
        DeleteDC(layer);
        return;
    }
    old_bitmap=SelectObject(layer,bitmap);
    old_font=SelectObject(layer,GetCurrentObject(dc,OBJ_FONT));
    ZeroMemory(pixels,(SIZE_T)width*height*sizeof(*pixels));
    SetBkMode(layer,TRANSPARENT);
    SetTextColor(layer,RGB(255,255,255));
    SetRect(&local,radius,radius,width-radius,height-radius);
    DrawTextW(layer,text,-1,&local,flags);
    GdiFlush();

    /* Blur the glyph coverage, then composite the text over the soft shadow. */
    for (y=0; y<height; y++)
    {
        DWORD *row=pixels+(SIZE_T)y*width;
        BYTE *blur=horizontal+(SIZE_T)y*width;
        int sum=0;
        for (x=0; x<=radius && x<width; x++)sum+=(row[x]>>8)&255;
        for (x=0; x<width; x++)
        {
            blur[x]=(BYTE)(sum/span);
            if (x>=radius)sum-=(row[x-radius]>>8)&255;
            if (x+radius+1<width)sum+=(row[x+radius+1]>>8)&255;
        }
    }
    for (x=0; x<width; x++)
    {
        int sum=0;
        for (y=0; y<=radius && y<height; y++)sum+=horizontal[(SIZE_T)y*width+x];
        for (y=0; y<height; y++)
        {
            SIZE_T i=(SIZE_T)y*width+x;
            DWORD foreground=(((pixels[i]>>8)&255)*alpha+127)/255;
            DWORD shadow=(112*alpha*(sum/span)+32767)/65025;
            DWORD opacity=foreground+((255-foreground)*shadow+127)/255;
            pixels[i]=(opacity<<24)|(foreground*0x010101);
            if (y>=radius)sum-=horizontal[i-(SIZE_T)radius*width];
            if (y+radius+1<height)sum+=horizontal[i+(SIZE_T)(radius+1)*width];
        }
    }

    AlphaBlend(dc,r.left-radius,r.top-radius,width,height,layer,0,0,width,height,blend);
    SelectObject(layer,old_font);
    SelectObject(layer,old_bitmap);
    HeapFree(GetProcessHeap(),0,horizontal);
    DeleteObject(bitmap);
    DeleteDC(layer);
}
static void fyto_draw_hover(HDC dc,const RECT *image,const RECT *canvas,const wchar_t *filename,
    int image_w,int image_h,int minimal,int playing,BYTE text_alpha,int show_play,int dpi_x,int dpi_y)
{
    fyto_hover_layout_t layout;wchar_t resolution[80];HGDIOBJ old_font;int saved,x,y,radius,pass,shift;
    fyto_hover_layout(image,canvas,dpi_x,dpi_y,&layout);
    saved=SaveDC(dc);IntersectClipRect(dc,canvas->left,canvas->top,canvas->right,canvas->bottom);
    if(!fyto_hover_font || fyto_hover_font_dpi!=dpi_y){
        if(fyto_hover_font)DeleteObject(fyto_hover_font);
        if(fyto_hover_caption_font)DeleteObject(fyto_hover_caption_font);
        fyto_hover_font=CreateFontW(-MulDiv(13,dpi_y,96),0,0,0,FW_NORMAL,0,0,0,
            DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,L"Source Han Sans CN");
        fyto_hover_caption_font=fyto_caption_create_font(dpi_y);fyto_hover_font_dpi=dpi_y;
    }
    old_font=SelectObject(dc,fyto_hover_caption_font?fyto_hover_caption_font:GetStockObject(DEFAULT_GUI_FONT));
    SetBkMode(dc,TRANSPARENT);
    if(minimal && filename)fyto_hover_text(dc,filename,layout.filename,
        DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX,text_alpha,dpi_y);
    SelectObject(dc,fyto_hover_font?fyto_hover_font:GetStockObject(DEFAULT_GUI_FONT));
    swprintf_s(resolution,80,L"%d × %d",image_w,image_h);
    fyto_hover_text(dc,resolution,layout.resolution,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX,text_alpha,dpi_y);
    x=(layout.play.left+layout.play.right)/2;y=(layout.play.top+layout.play.bottom)/2;
    radius=min(MulDiv(10,dpi_y,96),(layout.play.bottom-layout.play.top)/3);
    if(show_play && radius>=2){
        SelectObject(dc,GetStockObject(NULL_PEN));SelectObject(dc,GetStockObject(DC_BRUSH));
        for(pass=0;pass<2;pass++){
            shift=pass?0:1;SetDCBrushColor(dc,pass?RGB(255,255,255):RGB(0,0,0));
            if(playing){
                RECT bar;int gap=max(1,radius/3),bar_w=max(2,radius/2);
                SetRect(&bar,x-gap-bar_w+shift,y-radius+shift,x-gap+shift,y+radius+shift);
                FillRect(dc,&bar,(HBRUSH)GetStockObject(DC_BRUSH));
                OffsetRect(&bar,bar_w+gap*2,0);FillRect(dc,&bar,(HBRUSH)GetStockObject(DC_BRUSH));
            }else{
                POINT points[3]={{x-radius/2+shift,y-radius+shift},{x+radius+shift,y+shift},{x-radius/2+shift,y+radius+shift}};
                Polygon(dc,points,3);
            }
        }
    }
    SelectObject(dc,old_font);RestoreDC(dc,saved);
}
static void fyto_hover_presentation_destroy(void)
{
    if(fyto_hover_font){DeleteObject(fyto_hover_font);fyto_hover_font=NULL;}
    if(fyto_hover_caption_font){DeleteObject(fyto_hover_caption_font);fyto_hover_caption_font=NULL;}
}
#endif

