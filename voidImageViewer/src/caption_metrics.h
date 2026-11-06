#ifndef FYTO_CAPTION_METRICS_H
#define FYTO_CAPTION_METRICS_H
#include "fyto_text_render.h"
static void fyto_draw_ui_text(HDC dc,const wchar_t *text,int length,RECT *rect,UINT flags,int pixels)
{
    if (!fyto_text_render(dc,text,length,rect,flags,pixels))DrawTextW(dc,text,length,rect,flags);
}

/* Standard and minimal captions share their font and top-row alignment. */
static int fyto_caption_height(int dpi_y){return MulDiv(36,dpi_y,96);}
static int fyto_caption_font_pixels(int dpi_y){return MulDiv(10,dpi_y,72);}
static int fyto_menu_font_pixels(int dpi_y){return MulDiv(11,dpi_y,72);}
static HFONT fyto_create_ui_font(int pixels)
{
    return CreateFontW(-pixels,0,0,0,FW_NORMAL,0,0,0,
        DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
}
static HFONT fyto_caption_create_font(int dpi_y)
{
    return fyto_create_ui_font(fyto_caption_font_pixels(dpi_y));
}
static HFONT fyto_menu_create_font(int dpi_y)
{
    return fyto_create_ui_font(fyto_menu_font_pixels(dpi_y));
}
static void fyto_caption_filename_rect(const RECT *canvas,int dpi_x,int dpi_y,int menu_right,RECT *out)
{
    int margin=max(menu_right+12,MulDiv(44,dpi_x,96)*4+12);
    *out=*canvas;out->bottom=min(out->bottom,out->top+fyto_caption_height(dpi_y));
    out->left+=margin;out->right-=margin;
    if(out->right<=out->left)SetRectEmpty(out);
}
#endif
