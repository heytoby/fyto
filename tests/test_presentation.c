#include <windows.h>
#include <windowsx.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "../voidImageViewer/src/viewer_geometry.h"
#include "../voidImageViewer/src/hover_presentation.h"

static HWND _viv_hwnd=(HWND)(INT_PTR)1;
static BYTE _viv_in_sizemove,_viv_changing_frame,_viv_user_sized_window,_viv_is_fullscreen;
static BYTE _viv_fullscreen_is_maxed,_viv_fit_last_valid,_viv_fitting_window;
static RECT _viv_fit_last_rect,_viv_move_start_rect,_viv_fit_monitor_work,_viv_fullscreen_rect;
static HMONITOR _viv_fit_monitor;
static int _viv_fit_box_wide,_viv_fit_box_high;
static int _viv_frame_count,_viv_image_wide,_viv_image_high,fyto_formats_window;
static int os_logical_wide=96,os_logical_high=96,config_show_exif,config_show_caption;
static int config_left_click_action;
static int config_auto_zoom,config_auto_zoom_type;
static int _viv_frame_position,_viv_frame_looped,_viv_doing,_viv_is_slideshow;
static unsigned long long _viv_animation_timer_tick_start,_viv_timer_tick;
static int _viv_dst_pos_x=500,_viv_dst_pos_y=500,_viv_view_x,_viv_view_y;
static int _viv_dst_zoom_x_pos,_viv_dst_zoom_y_pos;
static float zoom_values[]={1.0f},*_viv_dst_zoom_values=zoom_values;
static WIN32_FIND_DATAW current_file,*_viv_current_fd=&current_file;
static RECT fake_window,fake_restore;
static POINT fake_cursor;
static MONITORINFO fake_monitor;
static int fake_zoomed,fake_iconic,fake_arranged,moves,toggles,native_drags,last_command,capture,occluded;
#define _VIV_DOING_1TO1SCROLL 5
#define VIV_ID_VIEW_WINDOW_SIZE_50 2000
#define VIV_ID_SLIDESHOW_PAUSE 1005
#define debug_printf(...) ((void)0)
static int title_height(void){return config_show_caption?36:0;}
static int _viv_get_status_high(void){return 0;}
static int _viv_get_controls_high(void){return 0;}
static BOOL _viv_window_arranged(HWND hwnd){(void)hwnd;return fake_arranged;}
static void _viv_toggle_fullscreen(void){
    toggles++;
    if(_viv_is_fullscreen){_viv_is_fullscreen=0;fake_window=_viv_fullscreen_rect;}
    else{_viv_fullscreen_rect=fake_window;_viv_is_fullscreen=1;fake_window=fake_monitor.rcMonitor;}
}
static BOOL WINAPI fake_get_window_rect(HWND hwnd,LPRECT rect){assert(hwnd==_viv_hwnd);*rect=fake_window;return TRUE;}
static BOOL WINAPI fake_get_monitor_info(HMONITOR handle,LPMONITORINFO info){(void)handle;*info=fake_monitor;return TRUE;}
static HMONITOR WINAPI fake_monitor_from_point(POINT point,DWORD flags){(void)point;(void)flags;return (HMONITOR)(INT_PTR)1;}
static HMONITOR WINAPI fake_monitor_from_window(HWND hwnd,DWORD flags){(void)hwnd;(void)flags;return (HMONITOR)(INT_PTR)1;}
static BOOL WINAPI fake_is_zoomed(HWND hwnd){(void)hwnd;return fake_zoomed;}
static BOOL WINAPI fake_is_iconic(HWND hwnd){(void)hwnd;return fake_iconic;}
static BOOL WINAPI fake_show_window(HWND hwnd,int show){(void)hwnd;assert(show==SW_RESTORE);fake_zoomed=fake_iconic=0;fake_window=fake_restore;return TRUE;}
static BOOL WINAPI fake_window_placement(HWND hwnd,WINDOWPLACEMENT *p){(void)hwnd;p->rcNormalPosition=fake_restore;return TRUE;}
static BOOL WINAPI fake_set_window_pos(HWND hwnd,HWND after,int x,int y,int w,int h,UINT flags){
    (void)after;(void)flags;assert(hwnd==_viv_hwnd);moves++;SetRect(&fake_window,x,y,x+w,y+h);fake_arranged=0;return TRUE;
}
static BOOL WINAPI fake_cursor_position(LPPOINT p){*p=fake_cursor;return TRUE;}
static HWND WINAPI fake_window_at_point(POINT p){(void)p;return occluded?(HWND)(INT_PTR)2:_viv_hwnd;}
static BOOL WINAPI fake_screen_to_client(HWND hwnd,LPPOINT p){(void)hwnd;p->x-=fake_window.left;p->y-=fake_window.top+title_height();return TRUE;}
static BOOL WINAPI fake_client_to_screen(HWND hwnd,LPPOINT p){(void)hwnd;p->x+=fake_window.left;p->y+=fake_window.top+title_height();return TRUE;}
static int WINAPI fake_metrics(int index){(void)index;return 4;}
static HWND WINAPI fake_set_capture(HWND hwnd){capture=1;return hwnd;}
static BOOL WINAPI fake_release_capture(void){capture=0;return TRUE;}
static LRESULT WINAPI fake_def_window_proc(HWND hwnd,UINT msg,WPARAM w,LPARAM l){(void)hwnd;(void)l;if(msg==WM_NCLBUTTONDOWN&&w==HTCAPTION)native_drags++;return 0;}
static BOOL WINAPI fake_invalidate(HWND hwnd,const RECT *r,BOOL erase){(void)hwnd;(void)r;(void)erase;return TRUE;}
static BOOL WINAPI fake_update(HWND hwnd){(void)hwnd;return TRUE;}
static BOOL WINAPI fake_post(HWND hwnd,UINT msg,WPARAM w,LPARAM l){(void)hwnd;(void)l;if(msg==WM_COMMAND)last_command=(int)w;return TRUE;}
static HCURSOR WINAPI fake_set_cursor(HCURSOR c){return c;}
static LPWSTR * WINAPI fake_command_args(LPCWSTR text,int *count){(void)text;*count=0;return NULL;}
static void _viv_viewport_rect(RECT *r){SetRect(r,0,0,fake_window.right-fake_window.left,fake_window.bottom-fake_window.top-title_height());}
static void _viv_get_render_size(int *w,int *h){
    RECT r;double s;_viv_viewport_rect(&r);s=(double)r.right/_viv_image_wide;
    if(s>(double)r.bottom/_viv_image_high)s=(double)r.bottom/_viv_image_high;
    *w=(int)(_viv_image_wide*s+0.5);*h=(int)(_viv_image_high*s+0.5);
}
static const wchar_t *string_get_filename_part(const wchar_t *p){const wchar_t *last=wcsrchr(p,L'\\');return last?last+1:p;}
static unsigned long long os_get_tick_count(void){return 100;}
static void _viv_timer_start(void){}
static void _viv_update_src_pixel(int a,int b){(void)a;(void)b;}
static void _viv_status_update(void){}
static void _viv_update_1to1_scroll(int x,int y){(void)x;(void)y;}
static void _viv_update_show_cursor(void){}
static void _viv_command(int id){(void)id;}
#undef GetMonitorInfo
#define GetMonitorInfo fake_get_monitor_info
#define GetWindowRect fake_get_window_rect
#define MonitorFromPoint fake_monitor_from_point
#define MonitorFromWindow fake_monitor_from_window
#define IsZoomed fake_is_zoomed
#define IsIconic fake_is_iconic
#define ShowWindow fake_show_window
#define GetWindowPlacement fake_window_placement
#define SetWindowPos fake_set_window_pos
#define GetCursorPos fake_cursor_position
#define WindowFromPoint fake_window_at_point
#define ScreenToClient fake_screen_to_client
#define ClientToScreen fake_client_to_screen
#define GetSystemMetrics fake_metrics
#define SetCapture fake_set_capture
#define ReleaseCapture fake_release_capture
#undef DefWindowProc
#define DefWindowProc fake_def_window_proc
#define InvalidateRect fake_invalidate
#define UpdateWindow fake_update
#undef PostMessage
#define PostMessage fake_post
#define SetCursor fake_set_cursor
#define CommandLineToArgvW fake_command_args
#include "../voidImageViewer/src/window_behavior.h"
#include "../voidImageViewer/src/hover_controls.h"
#include "../build/presentation-under-test.h"

static int same_rect(const RECT *a,const RECT *b){return !memcmp(a,b,sizeof(*a));}
static int inside(const RECT *r,const RECT *b){return r->left>=b->left&&r->top>=b->top&&r->right<=b->right&&r->bottom<=b->bottom;}
static void reset(void){
    _viv_in_sizemove=_viv_changing_frame=_viv_user_sized_window=_viv_is_fullscreen=0;
    _viv_fullscreen_is_maxed=_viv_fit_last_valid=_viv_fitting_window=0;_viv_fit_monitor=NULL;
    _viv_fit_box_wide=_viv_fit_box_high=0;_viv_frame_count=1;_viv_image_wide=6000;_viv_image_high=4000;
    config_show_caption=config_show_exif=config_auto_zoom=config_auto_zoom_type=config_left_click_action=0;
    _viv_is_slideshow=_viv_doing=fyto_formats_window=0;
    SetRect(&fake_monitor.rcMonitor,0,0,2560,1440);SetRect(&fake_monitor.rcWork,0,0,2560,1392);
    SetRect(&fake_window,100,100,1060,740);fake_restore=fake_window;_viv_fullscreen_rect=fake_window;
    fake_cursor.x=200;fake_cursor.y=200;
    fake_zoomed=fake_iconic=fake_arranged=moves=toggles=native_drags=last_command=capture=occluded=0;
    fyto_window_drag_pending=fyto_window_drag_from_image=fyto_image_hover=fyto_play_pressed=0;
    wcscpy_s(current_file.cFileName,MAX_PATH,L"Terrace-light_01.jpg");
}
static void save_bmp(const char *name,HDC dc,HBITMAP bitmap,int width,int height,void *pixels){
    BITMAPFILEHEADER f;BITMAPINFOHEADER b;FILE *file=NULL;(void)dc;(void)bitmap;
    memset(&f,0,sizeof(f));memset(&b,0,sizeof(b));b.biSize=sizeof(b);b.biWidth=width;b.biHeight=-height;
    b.biPlanes=1;b.biBitCount=32;b.biCompression=BI_RGB;f.bfType=0x4d42;f.bfOffBits=sizeof(f)+sizeof(b);
    f.bfSize=f.bfOffBits+width*height*4;assert(!fopen_s(&file,name,"wb"));
    fwrite(&f,sizeof(f),1,file);fwrite(&b,sizeof(b),1,file);fwrite(pixels,4,(size_t)width*height,file);fclose(file);
}
int main(void){
    int areas[][2]={{4096,4096},{2560,1392},{1080,1872},{800,552},{3840,2120}};
    int photos[][2]={{6000,4000},{4000,6000},{512,512},{1,1},{12000,1},{1,12000}};
    int a,p,cases=0;fyto_image_fit_t fit;RECT before,work,visible;POINT point;
    for(a=0;a<5;a++)for(p=0;p<6;p++){
        assert(fyto_default_display_fit(areas[a][0],areas[a][1],photos[p][0],photos[p][1],0,0,0,&fit));
        assert(fit.image_width<=areas[a][0]&&fit.image_height<=areas[a][1]);
        assert(fit.image_width<=2048&&fit.image_height<=2048);
        if(a==0)assert(max(fit.image_width,fit.image_height)==2048);
        cases++;
    }
    assert(!fyto_default_display_fit(100,36,10,10,36,0,0,&fit));
    reset();_viv_fit_image_window();
    assert(fake_window.right-fake_window.left==2048 && fake_window.bottom-fake_window.top==1365);cases++;
    reset();_viv_user_sized_window=1;SetRect(&fake_window,200,200,1400,660);before=fake_window;
    for(p=0;p<4;p++){_viv_image_wide=p%2?4000:6000;_viv_image_high=p%2?6000:4000;_viv_start_first_frame();assert(same_rect(&fake_window,&before));assert(!_viv_is_fullscreen&&!toggles);}
    cases++;
    reset();_viv_is_fullscreen=1;fake_window=fake_monitor.rcMonitor;
    fyto_window_drag_start.x=1280;fyto_window_drag_start.y=10;fyto_window_drag_origin=fake_window;
    point.x=1300;point.y=100;fyto_restore_for_drag(_viv_hwnd,point);before=fake_window;
    assert(!_viv_is_fullscreen&&_viv_user_sized_window&&toggles==1);
    for(p=0;p<3;p++){_viv_image_wide=4000;_viv_image_high=6000;_viv_start_first_frame();assert(!_viv_is_fullscreen&&same_rect(&fake_window,&before)&&toggles==1);}
    cases++;
    reset();_viv_is_fullscreen=1;fake_window=fake_monitor.rcMonitor;
    assert(fyto_image_body_drag(_viv_hwnd,MK_LBUTTON,MAKELPARAM(1280,720)));
    assert(fyto_window_drag_pending&&fyto_window_drag_from_image);
    fyto_window_drag(_viv_hwnd,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(1300,740));
    assert(!_viv_is_fullscreen&&_viv_user_sized_window&&native_drags==1);
    assert(abs((fake_window.left+fake_window.right)/2-1300)<=2);
    assert(abs((fake_window.top+fake_window.bottom)/2-740)<=2);cases++;
    reset();config_left_click_action=1;
    assert(!fyto_image_body_drag(_viv_hwnd,MK_LBUTTON,MAKELPARAM(400,300))&&!native_drags);cases++;
    for(p=0;p<2;p++){
        reset();_viv_is_fullscreen=1;fake_window=fake_monitor.rcMonitor;
        point.x=p?2559:0;point.y=1439;
        fyto_window_drag(_viv_hwnd,WM_NCLBUTTONDOWN,p?HTBOTTOMRIGHT:HTBOTTOMLEFT,MAKELPARAM(point.x,point.y));
        assert(!_viv_is_fullscreen&&_viv_user_sized_window&&inside(&fake_window,&fake_monitor.rcWork));
        before=fake_window;_viv_start_first_frame();assert(same_rect(&fake_window,&before)&&!_viv_is_fullscreen);cases++;
    }
    for(p=0;p<2;p++){
        reset();fake_zoomed=!p;fake_arranged=p;fake_window=fake_monitor.rcWork;
        _viv_fit_last_rect=fake_restore;_viv_fit_last_valid=1;
        point.x=2559;point.y=1391;
        fyto_window_drag(_viv_hwnd,WM_NCLBUTTONDOWN,HTBOTTOMRIGHT,MAKELPARAM(point.x,point.y));
        assert(!fake_zoomed&&!fake_arranged&&_viv_user_sized_window);
        before=fake_window;_viv_start_first_frame();assert(same_rect(&fake_window,&before));cases++;
    }
    reset();SetRect(&work,0,0,1920,1080);point.x=3;point.y=1077;
    assert(fyto_resize_hit(&work,point,5,16)==HTBOTTOMLEFT);
    point.x=1916;assert(fyto_resize_hit(&work,point,5,16)==HTBOTTOMRIGHT);cases++;

    reset();SetRect(&fake_window,0,0,960,640);_viv_image_wide=6000;_viv_image_high=4000;
    {
        HDC dc=CreateCompatibleDC(NULL);BITMAPINFO info;HBITMAP bitmap;HGDIOBJ old;DWORD *pixels;size_t bytes=960*640*4;
        DWORD *clean=(DWORD *)malloc(bytes);int x,y,different=0;fyto_hover_layout_t layout;
        memset(&info,0,sizeof(info));info.bmiHeader.biSize=sizeof(info.bmiHeader);info.bmiHeader.biWidth=960;info.bmiHeader.biHeight=-640;
        info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
        bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,(void **)&pixels,NULL,0);assert(bitmap&&clean);
        old=SelectObject(dc,bitmap);
        for(y=0;y<640;y++)for(x=0;x<960;x++){int gray=35+(x*120/960)+(y*45/640);pixels[y*960+x]=(gray<<16)|(gray<<8)|gray;}
        memcpy(clean,pixels,bytes);fake_cursor.x=-1;fake_cursor.y=-1;
        fyto_paint_image_hover(dc);assert(!memcmp(clean,pixels,bytes));save_bmp("build/clean-preview.bmp",dc,bitmap,960,640,pixels);
        fake_cursor.x=300;fake_cursor.y=300;occluded=1;
        fyto_paint_image_hover(dc);assert(!memcmp(clean,pixels,bytes));occluded=0;cases++;
        fake_cursor.x=300;fake_cursor.y=300;fyto_paint_image_hover(dc);
        for(y=0;y<640;y++)for(x=0;x<960;x++)if(clean[y*960+x]!=pixels[y*960+x])different++;
        assert(different>50 && different<10000);save_bmp("build/hover-preview.bmp",dc,bitmap,960,640,pixels);
        assert(fyto_visible_image_rect(&visible));fyto_hover_layout(&visible,96,96,&layout);
        point.x=(layout.play.left+layout.play.right)/2;point.y=(layout.play.top+layout.play.bottom)/2;
        assert(fyto_hover_input(_viv_hwnd,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(point.x,point.y)));
        assert(fyto_hover_input(_viv_hwnd,WM_LBUTTONUP,0,MAKELPARAM(point.x,point.y)));
        assert(last_command==VIV_ID_SLIDESHOW_PAUSE&&!capture);
        _viv_is_slideshow=1;memcpy(pixels,clean,bytes);fyto_paint_image_hover(dc);
        save_bmp("build/hover-playing-preview.bmp",dc,bitmap,960,640,pixels);
        memcpy(pixels,clean,bytes);
        fyto_draw_hover(dc,&visible,L"Hidden filename",6000,4000,0,0,96,96);
        for(y=0;y<80;y++)assert(!memcmp(clean+y*960,pixels+y*960,960*4));
        _viv_image_wide=6000;_viv_image_high=2000;
        fake_cursor.x=300;fake_cursor.y=20;
        assert(fyto_visible_image_rect(&work)&&!PtInRect(&work,fake_cursor));
        fyto_update_image_hover(fake_cursor);assert(!fyto_image_hover);
        memcpy(pixels,clean,bytes);fake_cursor.x=-1;fake_cursor.y=-1;fyto_paint_image_hover(dc);
        assert(!memcmp(clean,pixels,bytes));
        SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);free(clean);cases++;
    }
    fyto_hover_presentation_destroy();
    printf("presentation regressions passed: %d groups; free sizing, drag/next state, hover click and offscreen rendering\n",cases);
    return 0;
}

