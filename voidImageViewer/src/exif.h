#ifndef VIV_EXIF_H
#define VIV_EXIF_H
void exif_sync(HWND owner,const wchar_t *filename,int force);
void exif_shutdown(void);
int exif_sidebar_width(HWND owner);
void exif_set_background(COLORREF color);
void exif_layout(HWND owner,int top,int bottom);
#define VIV_ID_VIEW_EXIF 5001
#define VIV_ID_VIEW_TOGGLE_FIT_100 5002
#endif
