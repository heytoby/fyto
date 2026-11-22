#include <windows.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include "../voidImageViewer/src/bitmap_orientation.h"
static BOOL cancel(void *context){return ++*(int *)context>=2;}
int main(void){
    DWORD expected[8][6]={{1,2,3,4,5,6},{3,2,1,6,5,4},{6,5,4,3,2,1},{4,5,6,1,2,3},
        {1,4,2,5,3,6},{4,1,5,2,6,3},{6,3,5,2,4,1},{3,6,2,5,1,4}};
    BITMAPINFO info={0};HBITMAP image;DWORD *pixels,before,after;int i,j,k;
    info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=3;info.bmiHeader.biHeight=-2;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    image=CreateDIBSection(NULL,&info,DIB_RGB_COLORS,(void **)&pixels,NULL,0);assert(image);
    for(j=0;j<6;j++)pixels[j]=j+1;
    before=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
    for(k=0;k<1000;k++)for(i=1;i<=8;i++){
        DIBSECTION out={0};int calls=0;
        HBITMAP bitmap=fyto_orient_display_bitmap(image,i,NULL,NULL);assert(bitmap);
        assert(GetObject(bitmap,sizeof(out),&out));assert(out.dsBm.bmBits);
        assert(out.dsBm.bmWidth==(i>=5?2:3) && out.dsBm.bmHeight==(i>=5?3:2));
        for(j=0;j<6;j++)assert(((DWORD *)out.dsBm.bmBits)[j]==expected[i-1][j]);
        DeleteObject(bitmap);
        assert(!fyto_orient_display_bitmap(image,i,cancel,&calls));
    }
    after=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);assert(after==before);DeleteObject(image);
    puts("EXIF 1-8 exact pixel mappings passed; 8000 transforms and 8000 cancellations; stable GDI handles");return 0;
}
