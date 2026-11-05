#ifndef FYTO_CAPTION_METRICS_H
#define FYTO_CAPTION_METRICS_H
/* Standard and minimal captions share their font and top-row alignment. */
static int fyto_caption_height(int dpi_y){return MulDiv(36,dpi_y,96);}
static HFONT fyto_caption_create_font(int dpi_y)
{
    return CreateFontW(-MulDiv(10,dpi_y,72),0,0,0,FW_NORMAL,0,0,0,
        DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Source Han Sans CN");
}
static void fyto_caption_filename_rect(const RECT *canvas,int dpi_x,int dpi_y,int menu_right,RECT *out)
{
    int margin=max(menu_right+12,MulDiv(44,dpi_x,96)*3+12);
    *out=*canvas;out->bottom=min(out->bottom,out->top+fyto_caption_height(dpi_y));
    out->left+=margin;out->right-=margin;
    if(out->right<=out->left)SetRectEmpty(out);
}
#endif
