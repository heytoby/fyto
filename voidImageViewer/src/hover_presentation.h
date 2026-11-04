#ifndef FYTO_HOVER_PRESENTATION_H
#define FYTO_HOVER_PRESENTATION_H
#include "caption_metrics.h"
typedef struct {RECT filename,play,resolution;} fyto_hover_layout_t;
static HFONT fyto_hover_font,fyto_hover_caption_font;
static int fyto_hover_font_dpi;
static void fyto_hover_layout(const RECT *image,const RECT *canvas,int dpi_x,int dpi_y,fyto_hover_layout_t *out)
{
    int pad=MulDiv(12,dpi_x,96),box=MulDiv(32,dpi_y,96),text_h=MulDiv(20,dpi_y,96);
    int width=image->right-image->left,height=image->bottom-image->top;
    if(pad<2)pad=2;
    if(box>height-pad*2)box=max(0,height-pad*2);
    if(box>width-pad*2)box=max(0,width-pad*2);
    SetRect(&out->play,image->left+pad,image->bottom-pad-box,image->left+pad+box,image->bottom-pad);
    fyto_caption_filename_rect(canvas,dpi_x,dpi_y,0,&out->filename);
    SetRect(&out->resolution,out->play.right+pad,image->bottom-pad-text_h,image->right-pad,image->bottom-pad);
    if(out->filename.bottom+pad>out->play.top)SetRectEmpty(&out->filename);
    if(out->resolution.left>=out->resolution.right)SetRectEmpty(&out->resolution);
}
static void fyto_hover_text_solid(HDC dc,const wchar_t *text,RECT r,UINT flags)
{
    RECT shadow=r;
    OffsetRect(&shadow,1,1);SetTextColor(dc,RGB(0,0,0));
    DrawTextW(dc,text,-1,&shadow,flags);
    SetTextColor(dc,RGB(255,255,255));DrawTextW(dc,text,-1,&r,flags);
}
static void fyto_hover_text(HDC dc,const wchar_t *text,RECT r,UINT flags,BYTE alpha)
{
    HDC layer;HBITMAP bitmap;HGDIOBJ old_bitmap,old_font;RECT local;
    int width=r.right-r.left+1,height=r.bottom-r.top+1;BLENDFUNCTION blend={AC_SRC_OVER,0,0,0};
    if(!alpha || IsRectEmpty(&r))return;
    if(alpha==255){fyto_hover_text_solid(dc,text,r,flags);return;}
    layer=CreateCompatibleDC(dc);if(!layer)return;
    bitmap=CreateCompatibleBitmap(dc,width,height);
    if(!bitmap){DeleteDC(layer);return;}
    old_bitmap=SelectObject(layer,bitmap);
    old_font=SelectObject(layer,GetCurrentObject(dc,OBJ_FONT));SetBkMode(layer,TRANSPARENT);
    /* Copy the already painted image, then blend only the text difference. */
    BitBlt(layer,0,0,width,height,dc,r.left,r.top,SRCCOPY);
    SetRect(&local,0,0,width-1,height-1);fyto_hover_text_solid(layer,text,local,flags);
    blend.SourceConstantAlpha=alpha;
    AlphaBlend(dc,r.left,r.top,width,height,layer,0,0,width,height,blend);
    SelectObject(layer,old_font);SelectObject(layer,old_bitmap);DeleteObject(bitmap);DeleteDC(layer);
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
            DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,L"Segoe UI");
        fyto_hover_caption_font=fyto_caption_create_font(dpi_y);fyto_hover_font_dpi=dpi_y;
    }
    old_font=SelectObject(dc,fyto_hover_caption_font?fyto_hover_caption_font:GetStockObject(DEFAULT_GUI_FONT));
    SetBkMode(dc,TRANSPARENT);
    if(minimal && filename)fyto_hover_text(dc,filename,layout.filename,
        DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX,text_alpha);
    SelectObject(dc,fyto_hover_font?fyto_hover_font:GetStockObject(DEFAULT_GUI_FONT));
    swprintf_s(resolution,80,L"%d × %d",image_w,image_h);
    fyto_hover_text(dc,resolution,layout.resolution,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX,text_alpha);
    x=(layout.play.left+layout.play.right)/2;y=(layout.play.top+layout.play.bottom)/2;
    radius=min(MulDiv(7,dpi_y,96),(layout.play.bottom-layout.play.top)/3);
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

