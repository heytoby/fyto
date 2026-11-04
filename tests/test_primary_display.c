#include <windows.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

static BYTE _viv_in_sizemove,_viv_is_fullscreen,_viv_fit_last_valid;
static RECT _viv_fullscreen_rect,_viv_fit_monitor_work;
static HMONITOR _viv_fit_monitor;
static int _viv_fit_box_wide,_viv_fit_box_high;
static MONITORINFO primary_info;
static HMONITOR window_monitor;
static RECT window_rect,restore_rect;
static int zoomed,iconic,monitor_available,get_rect_ok,work_area_ok;
static int moves,restores,monitor_queries;
#define PRIMARY ((HMONITOR)(INT_PTR)1)
#define SECONDARY ((HMONITOR)(INT_PTR)2)
#define WINDOW ((HWND)(INT_PTR)3)

static HMONITOR WINAPI test_monitor_from_point(POINT point,DWORD flags){
    assert(point.x==0 && point.y==0);
    assert(flags==MONITOR_DEFAULTTOPRIMARY);
    monitor_queries++;return PRIMARY;
}
static BOOL WINAPI test_monitor_info(HMONITOR monitor,LPMONITORINFO info){
    assert(monitor==PRIMARY && info->cbSize==sizeof(*info));
    if(!monitor_available)return FALSE;
    *info=primary_info;return TRUE;
}
static HMONITOR WINAPI test_monitor_from_window(HWND hwnd,DWORD flags){
    assert(hwnd==WINDOW && flags==MONITOR_DEFAULTTONEAREST);return window_monitor;
}
static BOOL WINAPI test_system_parameters(UINT action,UINT unused,PVOID value,UINT flags){
    assert(action==SPI_GETWORKAREA);
    if(!work_area_ok)return FALSE;
    *(RECT *)value=primary_info.rcWork;return TRUE;
}
static int WINAPI test_system_metrics(int index){return index==SM_CXSCREEN?1920:1080;}
static BOOL WINAPI test_set_rect(LPRECT rect,int l,int t,int r,int b){
    rect->left=l;rect->top=t;rect->right=r;rect->bottom=b;return TRUE;
}
static BOOL WINAPI test_zoomed(HWND hwnd){return zoomed;}
static BOOL WINAPI test_iconic(HWND hwnd){return iconic;}
static BOOL WINAPI test_show_window(HWND hwnd,int command){
    assert(hwnd==WINDOW && command==SW_RESTORE && _viv_in_sizemove);
    restores++;zoomed=iconic=0;window_rect=restore_rect;return TRUE;
}
static BOOL WINAPI test_window_rect(HWND hwnd,LPRECT rect){
    assert(hwnd==WINDOW);if(!get_rect_ok)return FALSE;*rect=window_rect;return TRUE;
}
static BOOL WINAPI test_window_position(HWND hwnd,HWND after,int x,int y,int width,int height,UINT flags){
    assert(hwnd==WINDOW && after==NULL && _viv_in_sizemove);
    assert(flags==(SWP_NOZORDER|SWP_NOACTIVATE));moves++;
    test_set_rect(&window_rect,x,y,x+width,y+height);window_monitor=PRIMARY;return TRUE;
}
#undef GetMonitorInfo
#undef SystemParametersInfo
#define MonitorFromPoint test_monitor_from_point
#define GetMonitorInfo test_monitor_info
#define MonitorFromWindow test_monitor_from_window
#define SystemParametersInfo test_system_parameters
#define GetSystemMetrics test_system_metrics
#define SetRect test_set_rect
#define IsZoomed test_zoomed
#define IsIconic test_iconic
#define ShowWindow test_show_window
#define GetWindowRect test_window_rect
#define SetWindowPos test_window_position
#include "../voidImageViewer/src/primary_display.h"

static int same(const RECT *a,const RECT *b){return !memcmp(a,b,sizeof(*a));}
static int inside(const RECT *r,const RECT *area){
    return r->left>=area->left && r->top>=area->top &&
        r->right<=area->right && r->bottom<=area->bottom &&
        r->right>r->left && r->bottom>r->top;
}
static void reset(void){
    memset(&primary_info,0,sizeof(primary_info));primary_info.cbSize=sizeof(primary_info);
    SetRect(&primary_info.rcMonitor,0,0,1920,1080);
    SetRect(&primary_info.rcWork,0,0,1920,1040);
    SetRect(&window_rect,2200,100,3400,900);restore_rect=window_rect;
    SetRect(&_viv_fullscreen_rect,2100,100,3060,700);
    window_monitor=SECONDARY;zoomed=iconic=moves=restores=monitor_queries=0;
    monitor_available=get_rect_ok=work_area_ok=1;
    _viv_in_sizemove=_viv_is_fullscreen=0;_viv_fit_last_valid=1;
    _viv_fit_monitor=SECONDARY;_viv_fit_box_wide=_viv_fit_box_high=0;
}
int main(void){
    RECT work,before;int layouts,checks=0;
    reset();fyto_primary_work_rect(&work);
    assert(same(&work,&primary_info.rcWork) && monitor_queries==1);checks++;

    reset();fyto_open_image_on_primary(WINDOW);
    assert(moves==1 && !restores && inside(&window_rect,&primary_info.rcWork));
    assert(window_rect.left==360 && window_rect.top==120);
    assert(_viv_fit_monitor==PRIMARY && !_viv_fit_last_valid && !_viv_in_sizemove);
    assert(_viv_fit_box_wide==1920 && _viv_fit_box_high==1040);checks++;

    reset();window_monitor=PRIMARY;before=window_rect;
    fyto_open_image_on_primary(WINDOW);
    assert(!moves && !restores && same(&before,&window_rect));checks++;

    reset();_viv_is_fullscreen=1;fyto_open_image_on_primary(WINDOW);
    assert(_viv_is_fullscreen && !restores && moves==1);
    assert(same(&window_rect,&primary_info.rcMonitor));
    assert(inside(&_viv_fullscreen_rect,&primary_info.rcWork));checks++;

    reset();zoomed=1;fyto_open_image_on_primary(WINDOW);
    assert(restores==1 && !zoomed && inside(&window_rect,&primary_info.rcWork));checks++;
    reset();iconic=1;fyto_open_image_on_primary(WINDOW);
    assert(restores==1 && !iconic && inside(&window_rect,&primary_info.rcWork));checks++;

    for(layouts=0;layouts<3;layouts++){
        reset();
        if(layouts==0){SetRect(&primary_info.rcMonitor,0,0,1080,1920);SetRect(&primary_info.rcWork,40,0,1080,1872);}
        if(layouts==1){SetRect(&primary_info.rcMonitor,0,0,3840,2160);SetRect(&primary_info.rcWork,0,64,3840,2160);}
        if(layouts==2){SetRect(&primary_info.rcMonitor,0,0,800,600);SetRect(&primary_info.rcWork,0,0,800,552);}
        SetRect(&window_rect,-4000,-1500,-500,800);
        fyto_open_image_on_primary(WINDOW);
        assert(inside(&window_rect,&primary_info.rcWork));checks++;
    }
    reset();_viv_in_sizemove=1;fyto_open_image_on_primary(WINDOW);
    assert(_viv_in_sizemove==1 && moves==1);checks++;

    reset();get_rect_ok=0;fyto_open_image_on_primary(WINDOW);
    assert(!moves && !_viv_in_sizemove);checks++;
    reset();monitor_available=0;fyto_open_image_on_primary(WINDOW);
    assert(!moves);fyto_primary_work_rect(&work);
    assert(same(&work,&primary_info.rcWork));checks++;
    reset();monitor_available=work_area_ok=0;fyto_primary_work_rect(&work);
    assert(work.left==0 && work.top==0 && work.right==1920 && work.bottom==1080);checks++;
    reset();fyto_open_image_on_primary(NULL);assert(!monitor_queries && !moves);checks++;
    printf("primary-display regression checks passed: %d (mocked display APIs; no desktop changes)\n",checks);
    return 0;
}

