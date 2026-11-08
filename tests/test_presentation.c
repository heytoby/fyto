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
static BYTE _viv_fit_for_navigation,_viv_1to1,_viv_explicit_fit,_viv_have_old_zoom;
static int _viv_manual_long_edge,_viv_zoom_pos;
static double _viv_view_ix,_viv_view_iy;
static float _viv_zoom_presets[]={0.0f,1.0f};
static int config_fullscreen_fill_window,config_fill_window,config_keep_aspect_ratio,config_allow_shrinking;
static BYTE _viv_fullscreen_is_maxed,_viv_fit_last_valid,_viv_fitting_window;
static RECT _viv_fit_last_rect,_viv_move_start_rect,_viv_fit_monitor_work,_viv_fullscreen_rect;
static HMONITOR _viv_fit_monitor;
static int _viv_fit_box_wide,_viv_fit_box_high;
static int _viv_frame_count,_viv_image_wide,_viv_image_high,fyto_formats_window;
static int os_logical_wide=96,os_logical_high=96,config_show_exif,config_show_caption;
static int config_ontop,config_left_click_action,config_show_menu,config_show_thickframe,config_show_status,config_show_controls;
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
static int frame_positions,frame_layouts,frame_redraws;
static DWORD fake_now;
static UINT_PTR active_timer;
#define _VIV_DOING_1TO1SCROLL 5
#define _VIV_DST_ZOOM_ONE 0
#define VIV_ID_VIEW_WINDOW_SIZE_50 2000
#define VIV_ID_SLIDESHOW_PAUSE 1005
#define VIV_ID_VIEW_ONTOP_ALWAYS 1006
#define debug_printf(...) ((void)0)
static int title_height(void){return config_show_caption?36:0;}
static int _viv_get_status_high(void){return 0;}
static int _viv_get_controls_high(void){return 0;}
static BOOL _viv_window_arranged(HWND hwnd){(void)hwnd;return fake_arranged;}
static void _viv_toggle_fullscreen(void){
    toggles++;
    if (_viv_is_fullscreen)
    {
        if (_viv_changing_frame)frame_positions++;
        _viv_is_fullscreen=0;
        fake_window=_viv_fullscreen_rect;
    }
    else
    {
        _viv_fullscreen_rect=fake_window;
        _viv_is_fullscreen=1;
        fake_window=fake_monitor.rcMonitor;
    }
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
    RECT previous=fake_window;
    (void)after;assert(hwnd==_viv_hwnd);
    if (_viv_changing_frame)
    {
        assert((flags & SWP_FRAMECHANGED) && !(flags & SWP_NOCOPYBITS));
        assert(frame_positions==frame_redraws);
        frame_positions++;
    }
    if (flags & SWP_NOMOVE){x=previous.left;y=previous.top;}
    if (flags & SWP_NOSIZE){w=previous.right-previous.left;h=previous.bottom-previous.top;}
    SetRect(&fake_window,x,y,x+w,y+h);
    if (!EqualRect(&previous,&fake_window)){moves++;fake_arranged=0;}
    return TRUE;
}
static BOOL WINAPI fake_set_window_placement(HWND hwnd,const WINDOWPLACEMENT *p)
{
    RECT r=p->rcNormalPosition;
    assert(fake_zoomed && p->showCmd==SW_SHOWNORMAL);
    fake_zoomed=0;
    return fake_set_window_pos(hwnd,NULL,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_FRAMECHANGED);
}
static BOOL WINAPI fake_redraw(HWND hwnd,const RECT *rect,HRGN region,UINT flags)
{
    const UINT required=RDW_FRAME|RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW;
    assert(hwnd==_viv_hwnd && !rect && !region && (flags & required)==required);
    assert(!_viv_changing_frame && frame_positions==frame_redraws+1 && frame_layouts==frame_redraws+1);
    frame_redraws++;
    return TRUE;
}
static LONG WINAPI fake_get_window_long(HWND hwnd,int index){(void)hwnd;(void)index;return WS_CAPTION|WS_THICKFRAME|WS_SYSMENU|WS_MAXIMIZEBOX|WS_MINIMIZEBOX;}
static LONG WINAPI fake_set_window_long(HWND hwnd,int index,LONG style){(void)hwnd;(void)index;return style;}
static HMENU WINAPI fake_get_menu(HWND hwnd){(void)hwnd;return NULL;}
static BOOL WINAPI fake_set_menu(HWND hwnd,HMENU menu){(void)hwnd;(void)menu;return TRUE;}
static BOOL WINAPI fake_cursor_position(LPPOINT p){*p=fake_cursor;return TRUE;}
static HWND WINAPI fake_window_at_point(POINT p){return occluded||!PtInRect(&fake_window,p)?(HWND)(INT_PTR)2:_viv_hwnd;}
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
static DWORD WINAPI fake_tick_count(void){return fake_now;}
static UINT_PTR WINAPI fake_set_timer(HWND hwnd,UINT_PTR id,UINT period,TIMERPROC proc){(void)hwnd;(void)proc;assert(period==16);active_timer=id;return id;}
static BOOL WINAPI fake_kill_timer(HWND hwnd,UINT_PTR id){(void)hwnd;if(active_timer==id)active_timer=0;return TRUE;}
static LPWSTR * WINAPI fake_command_args(LPCWSTR text,int *count){(void)text;*count=0;return NULL;}
static void _viv_viewport_rect(RECT *r){int w=fake_window.right-fake_window.left;SetRect(r,0,0,w-(config_show_exif?min(340,w/2):0),fake_window.bottom-fake_window.top-title_height());}
#include "../build/render-under-test.h"
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
#define SetWindowPlacement fake_set_window_placement
#define SetWindowPos fake_set_window_pos
#define RedrawWindow fake_redraw
#undef GetWindowLong
#define GetWindowLong fake_get_window_long
#undef SetWindowLong
#define SetWindowLong fake_set_window_long
#define GetMenu fake_get_menu
#define SetMenu fake_set_menu
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
#define GetTickCount fake_tick_count
#define SetTimer fake_set_timer
#define KillTimer fake_kill_timer
#define CommandLineToArgvW fake_command_args
#include "../voidImageViewer/src/window_behavior.h"
#include "../voidImageViewer/src/hover_controls.h"
static int exif_sidebar_width(HWND hwnd)
{
    int width=fake_window.right-fake_window.left;
    (void)hwnd;
    return config_show_exif?min(340,width/2):0;
}
static void os_MonitorRectFromWindow(HWND hwnd,int fullscreen,RECT *rect)
{
    (void)hwnd;
    *rect=fullscreen?fake_monitor.rcMonitor:fake_monitor.rcWork;
}
static void _viv_on_size(void){assert(!_viv_changing_frame);frame_layouts++;}
static void _viv_status_show(int show){(void)show;}
static void _viv_controls_show(int show){(void)show;}
#include "../build/presentation-under-test.h"

static int same_rect(const RECT *a,const RECT *b){return !memcmp(a,b,sizeof(*a));}
static int inside(const RECT *r,const RECT *b){return r->left>=b->left&&r->top>=b->top&&r->right<=b->right&&r->bottom<=b->bottom;}
static unsigned long long contrast(const DWORD *a,const DWORD *b,int width,RECT r){
    unsigned long long sum=0;int x,y,c;for(y=r.top;y<r.bottom;y++)for(x=r.left;x<r.right;x++)
        for(c=0;c<3;c++)sum+=abs((int)((a[y*width+x]>>(c*8))&255)-(int)((b[y*width+x]>>(c*8))&255));
    return sum;
}
static void reset(void){
    _viv_in_sizemove=_viv_changing_frame=_viv_user_sized_window=_viv_is_fullscreen=0;
    _viv_fullscreen_is_maxed=_viv_fit_last_valid=_viv_fitting_window=0;_viv_fit_monitor=NULL;
    _viv_fit_box_wide=_viv_fit_box_high=0;_viv_frame_count=1;_viv_image_wide=6000;_viv_image_high=4000;
    _viv_manual_long_edge=_viv_zoom_pos=0;
    _viv_fit_for_navigation=_viv_1to1=_viv_explicit_fit=_viv_have_old_zoom=0;
    _viv_dst_pos_x=_viv_dst_pos_y=500;_viv_dst_zoom_x_pos=_viv_dst_zoom_y_pos=0;
    _viv_view_x=_viv_view_y=0;_viv_view_ix=_viv_view_iy=0.0;
    config_fullscreen_fill_window=config_fill_window=config_keep_aspect_ratio=config_allow_shrinking=1;
    config_show_caption=config_show_exif=config_auto_zoom=config_auto_zoom_type=config_left_click_action=0;
    _viv_is_slideshow=_viv_doing=fyto_formats_window=0;
    SetRect(&fake_monitor.rcMonitor,0,0,2560,1440);SetRect(&fake_monitor.rcWork,0,0,2560,1392);
    SetRect(&fake_window,100,100,1060,740);fake_restore=fake_window;_viv_fullscreen_rect=fake_window;
    fake_cursor.x=200;fake_cursor.y=200;
    fake_zoomed=fake_iconic=fake_arranged=moves=toggles=native_drags=last_command=capture=occluded=0;
    frame_positions=frame_layouts=frame_redraws=0;
    fyto_window_drag_pending=fyto_window_drag_from_image=fyto_image_hover=fyto_hover_pressed=0;config_ontop=0;
    ZeroMemory(&fyto_hover_fade,sizeof(fyto_hover_fade));fake_now=1000;active_timer=0;
    wcscpy_s(current_file.cFileName,MAX_PATH,L"Terrace-light_01.jpg");
}
static void save_bmp(const char *name,HDC dc,HBITMAP bitmap,int width,int height,void *pixels){
    BITMAPFILEHEADER f;BITMAPINFOHEADER b;FILE *file=NULL;(void)dc;(void)bitmap;
    memset(&f,0,sizeof(f));memset(&b,0,sizeof(b));b.biSize=sizeof(b);b.biWidth=width;b.biHeight=-height;
    b.biPlanes=1;b.biBitCount=32;b.biCompression=BI_RGB;f.bfType=0x4d42;f.bfOffBits=sizeof(f)+sizeof(b);
    f.bfSize=f.bfOffBits+width*height*4;assert(!fopen_s(&file,name,"wb"));
    fwrite(&f,sizeof(f),1,file);fwrite(&b,sizeof(b),1,file);fwrite(pixels,4,(size_t)width*height,file);fclose(file);
}
static void verify_text_rendering(void)
{
    enum {width=240,height=32};
    BITMAPINFO info={0};
    HDC dc=CreateCompatibleDC(NULL);
    HBITMAP bitmap;
    HGDIOBJ previous;
    DWORD *pixels;
    RECT area={0,0,width,height},measured={0};
    int left=0,right=0,edges=0;

    info.bmiHeader.biSize=sizeof(info.bmiHeader);
    info.bmiHeader.biWidth=width;
    info.bmiHeader.biHeight=-height;
    info.bmiHeader.biPlanes=1;
    info.bmiHeader.biBitCount=32;
    bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,(void **)&pixels,NULL,0);
    assert(dc && bitmap);
    previous=SelectObject(dc,bitmap);
    for (int i=0; i<width*height; i++)pixels[i]=0x00eeeeee;
    SetTextColor(dc,RGB(0,0,0));

    assert(fyto_text_render(dc,L"命令行选项(&C)",-1,&measured,DT_CALCRECT|DT_SINGLELINE,15));
    assert(measured.right>50 && measured.right<130);
    assert(fyto_text_render(dc,L"命令行选项(&C)",-1,&area,DT_VCENTER|DT_SINGLELINE,15));
    assert(fyto_text_render(dc,L"Ctrl+F1",-1,&area,DT_VCENTER|DT_SINGLELINE|DT_RIGHT|DT_NOPREFIX,15));
    GdiFlush();
    assert((pixels[width-1]&0x00ffffff)==0x00eeeeee);
    for (int y=0; y<height; y++)
        for (int x=0; x<width; x++)
        {
            DWORD rgb=pixels[y*width+x]&0x00ffffff;
            if (rgb==0x00eeeeee)continue;
            assert(rgb==(rgb&255)*0x010101);
            if (x<width/2)left++;
            else right++;
            if (rgb>0 && rgb<0x00eeeeee)edges++;
        }
    assert(left>150 && right>70 && edges>80);
    SelectObject(dc,previous);
    DeleteObject(bitmap);
    DeleteDC(dc);
    puts("DirectWrite Chinese menu and shortcut preserve background with grayscale edge coverage");
}
int wmain(void){
    assert(fyto_text_initialize());
    verify_text_rendering();
    int areas[][2]={{4096,4096},{2560,1392},{1080,1872},{800,552},{3840,2120}};
    int photos[][2]={{6000,4000},{4000,6000},{512,512},{1,1},{12000,1},{1,12000}};
    int a,p,cases=0;fyto_image_fit_t fit;RECT before,work,visible,canvas;POINT point;
    for(a=0;a<5;a++)for(p=0;p<6;p++){
        assert(fyto_default_display_fit(areas[a][0],areas[a][1],photos[p][0],photos[p][1],0,0,0,&fit));
        assert(fit.image_width<=areas[a][0]&&fit.image_height<=areas[a][1]);
        assert(max(fit.image_width,fit.image_height)==min(1980,min(areas[a][0],areas[a][1])));
        cases++;
    }
    assert(!fyto_default_display_fit(100,36,10,10,36,0,0,&fit));
    assert(!fyto_fixed_tile(-1920,0,0,1040,-1920,0,0,1040,12));
    assert(fyto_fixed_tile(-1920,0,-960,1040,-1920,0,0,1040,12));
    cases++;
    /* A title row reduces a screen-height picture; minimal mode reuses that height. */
    reset();
    SetRect(&fake_monitor.rcWork,0,0,1920,1040);
    SetRect(&fake_monitor.rcMonitor,0,0,1920,1080);
    _viv_image_wide=1920;_viv_image_high=1040;
    fake_window=fake_monitor.rcWork;
    for (a=0; a<3; a++)
    {
        int rw,rh;
        _viv_set_view_mode(TRUE);
        assert(config_show_caption && fake_window.left>0 && fake_window.right<1920);
        _viv_get_render_size(&rw,&rh);
        assert(rw==fake_window.right-fake_window.left && rh==1040-title_height());
        _viv_set_view_mode(FALSE);
        assert(!config_show_caption && same_rect(&fake_window,&fake_monitor.rcWork));
        _viv_get_render_size(&rw,&rh);
        assert(rw==1920 && rh==1040);
        cases++;
    }
    reset();
    fake_window=fake_monitor.rcMonitor;_viv_is_fullscreen=1;
    _viv_image_wide=2560;_viv_image_high=1440;
    _viv_set_view_mode(TRUE);
    assert(!_viv_is_fullscreen && toggles==1 && inside(&fake_window,&fake_monitor.rcWork));
    assert(fake_window.left>0 && fake_window.right<fake_monitor.rcWork.right);
    cases++;
    reset();
    fake_zoomed=1;fake_window=fake_monitor.rcWork;config_show_caption=1;
    _viv_image_wide=2560;_viv_image_high=1392;
    _viv_set_view_mode(FALSE);
    assert(fake_zoomed && moves==0 && same_rect(&fake_window,&fake_monitor.rcWork));
    cases++;
    reset();
    fake_zoomed=1;fake_window=fake_monitor.rcWork;config_show_caption=1;
    _viv_set_view_mode(FALSE);
    assert(!fake_zoomed && moves==1 && inside(&fake_window,&fake_monitor.rcWork));
    {
        int rw,rh;
        _viv_get_render_size(&rw,&rh);
        assert(rw==fake_window.right-fake_window.left && rh==fake_window.bottom-fake_window.top);
    }
    cases++;
    reset();
    fake_arranged=1;SetRect(&fake_window,0,0,1280,1392);before=fake_window;
    _viv_set_view_mode(TRUE);
    _viv_set_view_mode(FALSE);
    assert(fake_arranged && moves==0 && same_rect(&fake_window,&before));
    cases++;
    reset();
    before=fake_window;
    _viv_set_view_mode(TRUE);
    assert(config_show_caption && fake_window.left==before.left &&
           fake_window.top+title_height()==before.top && fake_window.bottom==before.bottom);
    _viv_set_view_mode(FALSE);
    assert(moves==2 && frame_redraws==2 && same_rect(&fake_window,&before) && !_viv_user_sized_window);
    cases++;
    /* Removing the title row trims both kinds of letterbox around the old image. */
    for (p=0; p<2; p++)
    {
        reset();
        config_show_caption=1;
        _viv_user_sized_window=1;
        if (p==0)_viv_image_wide=2000;
        else _viv_image_high=2000;
        SetRect(&fake_window,100,100,1300,1000);
        assert(fyto_visible_image_rect(&visible));
        OffsetRect(&visible,fake_window.left,fake_window.top+title_height());
        for (a=0; a<3; a++)
        {
            _viv_set_view_mode(FALSE);
            assert(same_rect(&fake_window,&visible));
            _viv_set_view_mode(TRUE);
            assert(fake_window.left==visible.left && fake_window.right==visible.right &&
                   fake_window.top+title_height()==visible.top && fake_window.bottom==visible.bottom);
        }
        cases++;
    }
    /* A narrow portrait image also refits the EXIF panel to the new width. */
    reset();
    config_show_caption=config_show_exif=1;
    _viv_image_wide=1000;_viv_image_high=5000;
    SetRect(&fake_window,100,100,1300,1000);
    assert(fyto_visible_image_rect(&visible));
    OffsetRect(&visible,fake_window.left,fake_window.top+title_height());
    _viv_set_view_mode(FALSE);
    assert(fake_window.left==visible.left && fake_window.top==visible.top && fake_window.bottom==visible.bottom);
    _viv_viewport_rect(&canvas);
    assert(canvas.right-canvas.left==visible.right-visible.left);
    cases++;
    /* Zoom and fixed tiling are still controlled by the viewer and Windows. */
    reset();
    _viv_zoom_pos=1;
    before=fake_window;
    _viv_set_view_mode(TRUE);
    assert(config_show_caption && moves==0 && same_rect(&fake_window,&before));
    cases++;

    reset();_viv_fit_image_window();
    assert(fake_window.right-fake_window.left==1392 && fake_window.bottom-fake_window.top==928);cases++;
    reset();_viv_user_sized_window=1;SetRect(&fake_window,200,200,1400,660);before=fake_window;
    _viv_fit_image_window();assert(same_rect(&fake_window,&before));
    {
        int rw,rh,bw,bh;_viv_get_render_size(&rw,&rh);_viv_start_first_frame();_viv_get_render_size(&bw,&bh);
        assert(bw==rw&&bh==rh);assert(fake_window.right-fake_window.left==rw&&fake_window.bottom-fake_window.top==rh);
        assert(_viv_manual_long_edge==690);
        for(p=0;p<20;p++){
            _viv_image_wide=p%2?4000:6000;_viv_image_high=p%2?6000:4000;
            rw=p%2?460:690;rh=p%2?690:460;_viv_start_first_frame();_viv_get_render_size(&bw,&bh);
            assert(rw==bw&&rh==bh);assert(fake_window.right-fake_window.left==rw&&fake_window.bottom-fake_window.top==rh);
            assert(!_viv_is_fullscreen&&!toggles);
        }
    }
    cases++;
    reset();config_show_caption=config_show_exif=1;_viv_user_sized_window=1;SetRect(&fake_window,100,100,1500,936);
    {
        int rw,rh,bw,bh;_viv_get_render_size(&rw,&rh);_viv_start_first_frame();_viv_get_render_size(&bw,&bh);
        assert(rw==bw&&rh==bh);assert(fake_window.right-fake_window.left==rw+min(340,rw));
        assert(fake_window.bottom-fake_window.top==rh+36);
        before=fake_window;_viv_start_first_frame();assert(same_rect(&before,&fake_window));
        _viv_image_wide=4000;_viv_image_high=6000;_viv_start_first_frame();_viv_get_render_size(&bw,&bh);
        assert(max(bw,bh)==max(rw,rh));assert(fake_window.right-fake_window.left==bw+min(340,bw));
        assert(fake_window.bottom-fake_window.top==bh+36);
    }
    cases++;
    reset();_viv_frame_count=_viv_image_wide=_viv_image_high=0;_viv_remember_manual_size();
    assert(_viv_manual_long_edge==1980);cases++;
    /* Fresh normal windows use the same 1980 long edge for every orientation. */
    for(p=0;p<3;p++){
        int rw,rh;reset();SetRect(&fake_monitor.rcMonitor,0,0,3840,2160);SetRect(&fake_monitor.rcWork,0,0,3840,2120);
        _viv_image_wide=p==0?6000:4000;_viv_image_high=p==1?6000:4000;
        _viv_start_first_frame();_viv_get_render_size(&rw,&rh);
        assert(max(rw,rh)==1980&&inside(&fake_window,&fake_monitor.rcWork));cases++;
    }
    /* Small screens use a common limit, including a portrait desktop. */
    for(a=0;a<2;a++){
        int first=0;reset();if(a)SetRect(&fake_monitor.rcWork,0,0,1080,1872);
        for(p=0;p<2;p++){
            int rw,rh;_viv_image_wide=p?4000:6000;_viv_image_high=p?6000:4000;
            _viv_start_first_frame();_viv_get_render_size(&rw,&rh);
            if(!p)first=max(rw,rh);else assert(max(rw,rh)==first);
            assert(inside(&fake_window,&fake_monitor.rcWork));
        }
        cases++;
    }
    reset();_viv_is_fullscreen=1;fake_window=fake_monitor.rcMonitor;
    fyto_window_drag_start.x=1280;fyto_window_drag_start.y=10;fyto_window_drag_origin=fake_window;
    point.x=1300;point.y=100;fyto_restore_for_drag(_viv_hwnd,point);before=fake_window;
    assert(!_viv_is_fullscreen&&_viv_user_sized_window&&toggles==1);
    _viv_image_wide=4000;_viv_image_high=6000;_viv_start_first_frame();before=fake_window;
    for(p=0;p<3;p++){_viv_start_first_frame();assert(!_viv_is_fullscreen&&same_rect(&fake_window,&before)&&toggles==1);}
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
    /* Fullscreen, maximized, snapped and height/width-filled ordinary windows. */
    for(a=0;a<5;a++)for(p=0;p<2;p++){
        int rw,rh;reset();SetRect(&fake_monitor.rcMonitor,0,0,3840,2160);SetRect(&fake_monitor.rcWork,0,0,3840,2120);
        _viv_image_wide=p?4000:6000;_viv_image_high=p?6000:4000;
        _viv_is_fullscreen=a==0;fake_zoomed=a==1;fake_arranged=a==2;
        if(a==3)SetRect(&fake_window,400,0,1600,2120);
        else if(a==4)SetRect(&fake_window,0,200,3840,1200);
        else fake_window=fake_monitor.rcWork;
        before=fake_window;assert(fyto_window_needs_drag_restore(_viv_hwnd));
        point.x=(fake_window.left+fake_window.right)/2;point.y=fake_window.top+18;
        assert(fyto_window_drag(_viv_hwnd,WM_NCLBUTTONDOWN,HTCAPTION,MAKELPARAM(point.x,point.y)));
        assert(same_rect(&before,&fake_window));
        assert(fyto_window_drag(_viv_hwnd,WM_LBUTTONUP,0,0));assert(same_rect(&before,&fake_window));
        assert(fyto_window_drag(_viv_hwnd,WM_NCLBUTTONDOWN,HTCAPTION,MAKELPARAM(point.x,point.y)));
        point.x+=12;point.y+=80;ScreenToClient(_viv_hwnd,&point);
        assert(fyto_window_drag(_viv_hwnd,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(point.x,point.y)));
        _viv_get_render_size(&rw,&rh);
        assert(max(rw,rh)==1980&&_viv_manual_long_edge==1980);
        assert(!_viv_is_fullscreen&&!fake_zoomed&&!fake_arranged&&native_drags==1);
        assert(inside(&fake_window,&fake_monitor.rcWork));
        _viv_image_wide=p?6000:4000;_viv_image_high=p?4000:6000;
        _viv_start_first_frame();_viv_get_render_size(&rw,&rh);assert(max(rw,rh)==1980&&!_viv_is_fullscreen);cases++;
    }
    for(p=0;p<2;p++){
        int rw,rh;reset();config_show_caption=p;
        SetRect(&fake_monitor.rcMonitor,0,0,3840,2160);SetRect(&fake_monitor.rcWork,0,0,3840,2120);
        _viv_is_fullscreen=!p;fake_zoomed=p;fake_window=fake_monitor.rcMonitor;
        _viv_zoom_pos=1;_viv_have_old_zoom=1;_viv_view_x=120;
        assert(!fyto_image_body_drag(_viv_hwnd,MK_LBUTTON,MAKELPARAM(1800,900)));
        assert(fyto_image_body_drag(_viv_hwnd,MK_LBUTTON,MAKELPARAM(1800,20)));
        assert(fyto_window_drag(_viv_hwnd,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(1810,100)));
        _viv_get_render_size(&rw,&rh);assert(max(rw,rh)==1980&&!_viv_zoom_pos&&!_viv_have_old_zoom&&!_viv_view_x);cases++;
    }
    reset();_viv_manual_long_edge=700;assert(!fyto_window_needs_drag_restore(_viv_hwnd));
    assert(!fyto_window_drag(_viv_hwnd,WM_NCLBUTTONDOWN,HTCAPTION,MAKELPARAM(400,110)));
    assert(_viv_manual_long_edge==700&&!capture);cases++;
    reset();SetRect(&work,0,0,1920,1080);point.x=3;point.y=1077;
    assert(fyto_resize_hit(&work,point,5,16)==HTBOTTOMLEFT);
    point.x=1916;assert(fyto_resize_hit(&work,point,5,16)==HTBOTTOMRIGHT);cases++;
    for(a=96;a<=192;a+=48){
        RECT title;LOGFONTW font;HFONT handle=fyto_caption_create_font(a);
        SetRect(&work,0,0,MulDiv(960,a,96),MulDiv(640,a,96));
        fyto_caption_filename_rect(&work,a,a,0,&title);
        assert(title.left+title.right==work.right&&title.top==0&&title.bottom==MulDiv(36,a,96));
        assert(GetObjectW(handle,sizeof(font),&font)==sizeof(font));assert(font.lfHeight==-MulDiv(10,a,72));
        assert(!wcscmp(font.lfFaceName,L"Microsoft YaHei UI"));DeleteObject(handle);cases++;
    }
    {
        fyto_hover_fade_t fade={0};int alpha=0,next;
        fyto_hover_fade_to(&fade,255,1000);assert(!fyto_hover_fade_alpha(&fade,1000));
        for(a=0;a<=140;a+=7){next=fyto_hover_fade_alpha(&fade,1000+a);assert(next>=alpha&&next<=255);alpha=next;}
        assert(alpha==255);fyto_hover_fade_to(&fade,0,1140);
        alpha=fyto_hover_fade_alpha(&fade,1230);assert(alpha>0&&alpha<255);
        fyto_hover_fade_to(&fade,255,1230);assert(fyto_hover_fade_alpha(&fade,1230)==alpha);
        assert(fyto_hover_fade_alpha(&fade,1370)==255);
        fyto_hover_fade_to(&fade,0,1370);assert(!fyto_hover_fade_alpha(&fade,1550));cases++;
        fyto_hover_fade_to(&fade,255,0xfffffff0UL);
        alpha=fyto_hover_fade_alpha(&fade,54);assert(alpha>0&&alpha<255);
        assert(fyto_hover_fade_alpha(&fade,124)==255);cases++;
    }

    reset();SetRect(&fake_window,0,0,960,640);_viv_image_wide=6000;_viv_image_high=4000;
    {
        HDC dc=CreateCompatibleDC(NULL);BITMAPINFO info;HBITMAP bitmap;HGDIOBJ old;DWORD *pixels;size_t bytes=960*640*4;
        DWORD *clean=(DWORD *)malloc(bytes),*full=(DWORD *)malloc(bytes);int x,y,different=0;fyto_hover_layout_t layout;
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
        assert(active_timer==FYTO_HOVER_FADE_TIMER);
        assert(fyto_visible_image_rect(&visible));_viv_viewport_rect(&canvas);fyto_hover_layout(&visible,&canvas,96,96,&layout);
        assert(!contrast(pixels,clean,960,layout.filename));
        assert(contrast(pixels,clean,960,layout.pin)>0);
        assert(abs((layout.pin.top+layout.pin.bottom)-(layout.filename.top+layout.filename.bottom))<=1);
        point.x=(layout.pin.left+layout.pin.right)/2;point.y=(layout.pin.top+layout.pin.bottom)/2;
        assert(fyto_hover_input(_viv_hwnd,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(point.x,point.y)));
        assert(fyto_hover_input(_viv_hwnd,WM_LBUTTONUP,0,MAKELPARAM(point.x,point.y)));
        assert(last_command==VIV_ID_VIEW_ONTOP_ALWAYS&&!capture);
        config_ontop=1;
        {
            RECT center={point.x-1,point.y-2,point.x+2,point.y+1};
            unsigned long long outline=contrast(pixels,clean,960,center);
            memcpy(pixels,clean,bytes);fyto_paint_image_hover(dc);
            assert(contrast(pixels,clean,960,center)>outline);
        }
        config_show_caption=1;
        assert(fyto_hover_command(point)!=VIV_ID_VIEW_ONTOP_ALWAYS);
        config_show_caption=0;
        cases++;
        fake_now+=30;point=fake_cursor;fyto_update_image_hover(point);assert(fyto_hover_fade.started==1000);
        fake_now+=120;fyto_hover_input(_viv_hwnd,WM_TIMER,FYTO_HOVER_FADE_TIMER,0);assert(!active_timer);
        memcpy(pixels,clean,bytes);fyto_paint_image_hover(dc);memcpy(full,pixels,bytes);
        for(y=0;y<640;y++)for(x=0;x<960;x++)if(clean[y*960+x]!=pixels[y*960+x])different++;
        assert(different>50 && different<10000);save_bmp("build/hover-preview.bmp",dc,bitmap,960,640,pixels);
        assert(fyto_visible_image_rect(&visible));_viv_viewport_rect(&canvas);fyto_hover_layout(&visible,&canvas,96,96,&layout);
        {
            int left=960,right=0,top=640,bottom=0;
            for(y=0;y<36;y++)for(x=layout.filename.left;x<layout.filename.right;x++)if(clean[y*960+x]!=pixels[y*960+x]){
                left=min(left,x);right=max(right,x);top=min(top,y);bottom=max(bottom,y);
            }
            assert(right>left&&abs((left+right)-960)<=5&&abs((top+bottom)-36)<=5);cases++;
        }
        assert(abs(layout.play.left+layout.play.right-visible.left-visible.right)<=1);
        assert(layout.play.right-layout.play.left==40);
        assert(layout.resolution.left>=layout.play.right);
        point.x=(layout.play.left+layout.play.right)/2;point.y=(layout.play.top+layout.play.bottom)/2;
        assert(fyto_hover_input(_viv_hwnd,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(point.x,point.y)));
        assert(fyto_hover_input(_viv_hwnd,WM_LBUTTONUP,0,MAKELPARAM(point.x,point.y)));
        assert(last_command==VIV_ID_SLIDESHOW_PAUSE&&!capture);
        _viv_is_slideshow=1;memcpy(pixels,clean,bytes);fyto_paint_image_hover(dc);
        save_bmp("build/hover-playing-preview.bmp",dc,bitmap,960,640,pixels);
        fake_cursor.x=-1;fake_cursor.y=-1;fyto_hover_input(_viv_hwnd,WM_MOUSELEAVE,0,0);fake_now+=90;
        memcpy(pixels,clean,bytes);fyto_paint_image_hover(dc);
        assert(contrast(pixels,clean,960,layout.filename)>0&&contrast(pixels,clean,960,layout.filename)<contrast(full,clean,960,layout.filename));
        assert(contrast(pixels,clean,960,layout.resolution)>0&&contrast(pixels,clean,960,layout.resolution)<contrast(full,clean,960,layout.resolution));
        assert(!contrast(pixels,clean,960,layout.play));save_bmp("build/hover-fade-preview.bmp",dc,bitmap,960,640,pixels);
        fake_now+=90;fyto_hover_input(_viv_hwnd,WM_TIMER,FYTO_HOVER_FADE_TIMER,0);assert(!active_timer);
        memcpy(pixels,clean,bytes);fyto_paint_image_hover(dc);assert(!memcmp(pixels,clean,bytes));cases++;
        memcpy(pixels,clean,bytes);
        fyto_draw_hover(dc,&visible,&canvas,L"Hidden filename",6000,4000,0,0,255,1,96,96);
        for(y=0;y<80;y++)assert(!memcmp(clean+y*960,pixels+y*960,960*4));
        _viv_image_wide=6000;_viv_image_high=2000;
        fake_cursor.x=300;fake_cursor.y=20;
        assert(fyto_visible_image_rect(&work)&&!PtInRect(&work,fake_cursor));
        fyto_update_image_hover(fake_cursor);assert(!fyto_image_hover);
        _viv_viewport_rect(&canvas);fyto_hover_layout(&work,&canvas,96,96,&layout);
        point.x=(layout.pin.left+layout.pin.right)/2;
        point.y=(layout.pin.top+layout.pin.bottom)/2;
        assert(!fyto_hover_command(point));
        fake_cursor.x=point.x;fake_cursor.y=work.top;
        fyto_update_image_hover(fake_cursor);assert(fyto_image_hover);
        fake_cursor.y=work.top-1;
        fyto_update_image_hover(fake_cursor);assert(fyto_image_hover);
        fake_cursor=point;
        fyto_update_image_hover(fake_cursor);assert(fyto_image_hover);
        assert(fyto_hover_command(point)==VIV_ID_VIEW_ONTOP_ALWAYS);
        fyto_hover_stop();
        memcpy(pixels,clean,bytes);fake_cursor.x=-1;fake_cursor.y=-1;fyto_paint_image_hover(dc);
        assert(!memcmp(clean,pixels,bytes));
        SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);free(clean);free(full);cases++;
    }
    fyto_hover_presentation_destroy();
    fyto_text_shutdown();
    printf("presentation regressions passed: %d groups; mode switching, long-edge sizing, drag restoration and hover controls\n",cases);
    return 0;
}

