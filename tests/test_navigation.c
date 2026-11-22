#define UNICODE
#define _UNICODE
#include <windows.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#define FYTO_NAV_PREVIOUS 1
#define FYTO_NAV_WHEEL 2
#define FYTO_NAV_WRAP 4
#define VIV_ID_SLIDESHOW_TIMER 1
static BYTE _viv_directory_steps[4096],_viv_navigation_in_flight;
static unsigned _viv_directory_step_count;
static WIN32_FIND_DATA file,*_viv_current_fd=&file,_viv_requested_fd;
static void *_viv_playlist_start,*_viv_directory_cache;
static int _viv_load_is_preload,_viv_random,_viv_wheel_navigation,_viv_navigation_batching;
static int _viv_dispatching_navigation,_viv_last_is_prev,_viv_is_slideshow,config_slideshow_rate;
static int _viv_wheel_delta,delay_ms,requested,shown,opens;
static HWND _viv_hwnd;
static struct {HBITMAP hbitmap;} _viv_preview;
static void _viv_next(int a,int b,int c,int d){assert(!"unexpected playlist/random path");}
static BOOL _viv_directory_changed(void *cache){return FALSE;}
static void _viv_schedule_directory_scan(void){}
static void _viv_finish_directory_navigation(void){if(!delay_ms){shown=requested;_viv_navigation_in_flight=0;}}
static void _viv_open(WIN32_FIND_DATA *next,int preload){
    _viv_requested_fd=*next;requested=_wtoi(next->cFileName);opens++;
    /* Busy decoders keep the old current file until they retire. */
    if(!delay_ms)file=*next;
}
static BOOL _viv_cached_directory_next(WIN32_FIND_DATA *from,int prev,BOOL wrap,WIN32_FIND_DATA *out,BOOL fresh){
    int n=_wtoi(from->cFileName)+(prev?-1:1);memset(out,0,sizeof(*out));
    if(n<1 || n>20){if(!wrap)return TRUE;n=n<1?20:1;}
    _itow_s(n,out->cFileName,MAX_PATH,10);return TRUE;
}
static void _viv_next_manual(int prev,int repeat);
static void _viv_replay_directory_navigation(BOOL fresh);
#include "../build/navigation-under-test.h"
static void reset(int delay,int initial){
    memset(&file,0,sizeof(file));_itow_s(initial,file.cFileName,MAX_PATH,10);
    _viv_requested_fd=file;
    _viv_directory_step_count=_viv_navigation_in_flight=0;_viv_wheel_delta=0;
    _viv_navigation_batching=_viv_wheel_navigation=0;
    requested=shown=initial;delay_ms=delay;opens=0;
}
int main(void){
    int delays[]={0,50,500},i,n,cases=0;
    for(i=0;i<3;i++){
        reset(delays[i],10);wheel(1,-120);wheel(1,120);wheel(1,120);assert(requested==9);cases++;
        reset(delays[i],10);for(n=0;n<12;n++)wheel(1,-10);assert(requested==11);cases++;
        reset(delays[i],10);wheel(1,-60);wheel(0,120);wheel(1,-60);assert(requested==11);cases++;
        reset(delays[i],20);wheel(1,-120);wheel(1,120);assert(requested==19);cases++;
        reset(delays[i],1);wheel(1,120);wheel(1,-120);assert(requested==2);cases++;
        reset(delays[i],10);wheel(2,-120);assert(requested==9);cases++;
        reset(delays[i],10);for(n=0;n<10;n++)_viv_next_manual(0,1);assert(requested==20);cases++;
        reset(delays[i],10);for(n=0;n<1000;n++){wheel(1,-120);wheel(1,120);}assert(requested==10);cases++;
    }
    printf("navigation: %d groups passed using production navigation functions; simulated decode delay 0/50/500 ms\n",cases);
    return 0;
}
