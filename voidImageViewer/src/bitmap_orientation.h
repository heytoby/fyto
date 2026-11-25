#ifndef FYTO_BITMAP_ORIENTATION_H
#define FYTO_BITMAP_ORIENTATION_H
/* Display JPEGs are top-down 32-bit DIBs. Keep that representation after
   EXIF orientation so scaling and ICC can read pixels without GDI copies. */
static HBITMAP fyto_orient_display_bitmap(HBITMAP input,int orientation,
    BOOL (*cancelled)(void *),void *context)
{
    DIBSECTION src={0};BITMAPINFO info={0};HBITMAP result;DWORD *pixels;
    int w,h,ow,oh,x,y;
    if(orientation<1 || orientation>8 || !GetObject(input,sizeof(src),&src) ||
        !src.dsBm.bmBits || src.dsBm.bmBitsPixel!=32)return NULL;
    /* GetObject normalizes biHeight to positive even for top-down DIBs.
       The caller supplies the top-down buffer created by fyto_image_dib. */
    w=src.dsBm.bmWidth;h=src.dsBm.bmHeight;
    if(w<=0 || h<=0 || (cancelled && cancelled(context)))return NULL;
    ow=orientation>=5?h:w;oh=orientation>=5?w:h;
    if((UINT64)ow*oh>MAXDWORD/4)return NULL;
    info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=ow;info.bmiHeader.biHeight=-oh;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    result=CreateDIBSection(NULL,&info,DIB_RGB_COLORS,(void **)&pixels,NULL,0);
    if(!result)return NULL;
    for(y=0;y<oh;y++){
        if(cancelled && cancelled(context)){DeleteObject(result);return NULL;}
        for(x=0;x<ow;x++){
            int sx=x,sy=y;
            switch(orientation){
                case 2:sx=w-1-x;break;
                case 3:sx=w-1-x;sy=h-1-y;break;
                case 4:sy=h-1-y;break;
                case 5:sx=y;sy=x;break;
                case 6:sx=y;sy=h-1-x;break;
                case 7:sx=w-1-y;sy=h-1-x;break;
                case 8:sx=w-1-y;sy=x;break;
            }
            pixels[(SIZE_T)y*ow+x]=*(DWORD *)((BYTE *)src.dsBm.bmBits+(SIZE_T)sy*src.dsBm.bmWidthBytes+(SIZE_T)sx*4);
        }
    }
    return result;
}
#endif
