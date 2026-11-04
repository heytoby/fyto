#include "fyto_formats.h"
// fyto presentation; opaque rectangular frame, without glass extension or a DWM shadow.
static void fyto_window_finish(HWND hwnd,int flat)
{
    typedef HRESULT (WINAPI *set_attr_t)(HWND,DWORD,LPCVOID,DWORD);
    typedef HRESULT (WINAPI *extend_t)(HWND,const int *);
    static HMODULE module; static set_attr_t attr; static extend_t extend; static HWND configured;
    int corners=1,policy=1,margins[4]={0,0,0,0};
    {
        static HWND region_hwnd; static int rw,rh,busy; RECT rect;
        if(hwnd && !busy && GetWindowRect(hwnd,&rect)) {
            int width=rect.right-rect.left,height=rect.bottom-rect.top;
            if(width>0 && height>0 && (region_hwnd!=hwnd || rw!=width || rh!=height)) {
                HRGN region=CreateRectRgn(0,0,width,height); busy=1;
                if(region && !SetWindowRgn(hwnd,region,TRUE))DeleteObject(region);
                region_hwnd=hwnd;rw=width;rh=height;busy=0;
            }
        }
    }
    if(!hwnd || hwnd==configured)return;
    configured=hwnd;
    if(!module){module=LoadLibraryW(L"dwmapi.dll");if(module){attr=(set_attr_t)GetProcAddress(module,"DwmSetWindowAttribute");extend=(extend_t)GetProcAddress(module,"DwmExtendFrameIntoClientArea");}}
    if(attr){attr(hwnd,33,&corners,sizeof(corners));attr(hwnd,2,&policy,sizeof(policy));}
    if(flat)margins[0]=margins[1]=margins[2]=margins[3]=0;
    if(extend)extend(hwnd,margins);
}
static HBITMAP fyto_splash_bitmap;
static LRESULT CALLBACK fyto_splash_proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp)
{
    if(msg==WM_TIMER || msg==WM_LBUTTONDOWN){DestroyWindow(hwnd);return 0;}
    if(msg==WM_ERASEBKGND)return 1;
    if(msg==WM_PAINT){PAINTSTRUCT ps;RECT r;BITMAP bm;HDC src;HGDIOBJ old;BeginPaint(hwnd,&ps);GetClientRect(hwnd,&r);GetObject(fyto_splash_bitmap,sizeof(bm),&bm);src=CreateCompatibleDC(ps.hdc);old=SelectObject(src,fyto_splash_bitmap);SetStretchBltMode(ps.hdc,HALFTONE);SetBrushOrgEx(ps.hdc,0,0,NULL);StretchBlt(ps.hdc,0,0,r.right,r.bottom,src,0,0,bm.bmWidth,bm.bmHeight,SRCCOPY);SelectObject(src,old);DeleteDC(src);EndPaint(hwnd,&ps);return 0;}
    if(msg==WM_DESTROY){KillTimer(hwnd,1);DeleteObject(fyto_splash_bitmap);fyto_splash_bitmap=NULL;return 0;}
    return DefWindowProcW(hwnd,msg,wp,lp);
}
static void fyto_show_splash(void)
{
    WNDCLASSW wc={0};BITMAP bm;RECT work;int width,height;HWND hwnd;
    HINSTANCE instance=GetModuleHandleW(NULL);
    fyto_splash_bitmap=(HBITMAP)LoadImageW(instance,MAKEINTRESOURCEW(60001),IMAGE_BITMAP,0,0,LR_CREATEDIBSECTION);
    if(!fyto_splash_bitmap)return;
    GetObject(fyto_splash_bitmap,sizeof(bm),&bm);SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
    width=min(bm.bmWidth,(work.right-work.left)*2/5);height=MulDiv(width,bm.bmHeight,bm.bmWidth);
    if(height>(work.bottom-work.top)*2/3){height=(work.bottom-work.top)*2/3;width=MulDiv(height,bm.bmWidth,bm.bmHeight);}
    wc.lpfnWndProc=fyto_splash_proc;wc.hInstance=instance;wc.lpszClassName=L"FYTO_SPLASH";wc.hCursor=LoadCursor(NULL,IDC_ARROW);RegisterClassW(&wc);
    hwnd=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_NOACTIVATE,L"FYTO_SPLASH",L"fyto",WS_POPUP,(work.left+work.right-width)/2,(work.top+work.bottom-height)/2,width,height,NULL,NULL,instance,NULL);
    if(!hwnd){DeleteObject(fyto_splash_bitmap);fyto_splash_bitmap=NULL;return;}
    fyto_window_finish(hwnd,1);ShowWindow(hwnd,SW_SHOWNOACTIVATE);UpdateWindow(hwnd);SetTimer(hwnd,1,700,NULL);
}
static void fyto_first_run(HWND owner)
{
    DWORD selected=0;
    fyto_startup_complete=1;
    fyto_offer_formats=!fyto_read_association_value(L"SelectedFormats",&selected);
    if(fyto_formats_requested)fyto_format_dialog(owner);
    InvalidateRect(owner,NULL,FALSE);
}
