from pathlib import Path
from test_playback_contract import body
import re
root=Path(__file__).resolve().parents[1]
source=(root/'voidImageViewer/src/viv.c').read_text(encoding='utf-8-sig')
wheel=body(source,'_viv_do_mousewheel_action')
start=wheel.index('    else if (action == 1 || action == 2)')
wheel='static void test_wheel(int action,int delta) {\nif(action==0){zoom_events++;return;}\n'+wheel[start:]+'\n'
# Include the operation-change guard that belongs to the production entry point.
wheel=wheel.replace('if(action==0)', '''if(action!=last_action){_viv_wheel_delta=0;last_action=action;}if(action==0)''')
case=source[source.index('\t\tcase WM_MOUSEWHEEL:'):]
case=case[:case.index('\n\t\tcase ',1)]
case=case[case.index('{')+1:case.rindex('}')]
case=case.replace('break;','').replace('_viv_do_mousewheel_action(','test_wheel_adapter(')
preamble=r'''
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <windowsx.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
enum { FYTO_NAV_PREVIOUS=1,FYTO_NAV_WRAP=2,FYTO_NAV_WHEEL=4 };
static WIN32_FIND_DATAW current,requested,shown,loading;
static WIN32_FIND_DATAW *_viv_current_fd=&current,*_viv_frame_fd=&shown,*_viv_load_fd=&loading;
#define _viv_requested_fd requested
static BYTE *_viv_directory_steps;
static unsigned _viv_directory_step_count,_viv_directory_step_capacity;
static BYTE _viv_navigation_in_flight,_viv_last_is_prev;
static BOOL _viv_dispatching_navigation;
static int _viv_wheel_navigation,_viv_wheel_delta;
static int last_action=-1,zoom_events,opens,delay_image,delay_directory;
static int _viv_load_is_preload,_viv_load_frame_count,_viv_load_image_terminate;
static int _viv_is_slideshow,_viv_preview_bitmap,config_slideshow_rate;
static int config_mouse_wheel_action=1,config_ctrl_mouse_wheel_action=0;
static struct {HBITMAP hbitmap;} _viv_preview;
static void *_viv_playlist_start,*_viv_random,*_viv_load_image_thread,*_viv_load_image_next_fd,*_viv_directory_cache;
static HWND _viv_hwnd;
static void *_viv_active_load;
#define VIV_ID_SLIDESHOW_TIMER 1
static void _viv_preload_next(void){}
static BOOL _viv_same_image(const WIN32_FIND_DATAW *a,const WIN32_FIND_DATAW *b){return !wcscmp(a->cFileName,b->cFileName);}
static int index_of(const WIN32_FIND_DATAW *file){return _wtoi(file->cFileName);}
static void select_file(WIN32_FIND_DATAW *file,int index){swprintf_s(file->cFileName,MAX_PATH,L"%04d",index);}
static BOOL _viv_cached_directory_next(const WIN32_FIND_DATAW *file,int prev,BOOL wrap,WIN32_FIND_DATAW *next,BOOL fresh){
 int index=index_of(file)+(prev?-1:1);(void)fresh;
 if(delay_directory)return FALSE;
 next->cFileName[0]=0;
 if(index<1||index>2000){if(!wrap)return TRUE;index=index<1?2000:1;}
 select_file(next,index);return TRUE;
}
static void _viv_schedule_directory_scan(void){}
static BOOL _viv_directory_changed(void *cache){(void)cache;return FALSE;}
static void _viv_next(int prev,int a,int b,int repeat){(void)a;(void)b;(void)repeat;select_file(&current,index_of(&current)+(prev?-1:1));}
static void _viv_open(WIN32_FIND_DATAW *file,int preload){
 (void)preload;current=requested=loading=*file;opens++;
 _viv_active_load=delay_image?(void *)1:NULL;
 if(!delay_image)shown=current;
}
static void _viv_replay_directory_navigation(BOOL fresh);
'''
parts=[body(source,name) for name in ['_viv_finish_directory_navigation','_viv_replay_directory_navigation','_viv_next_manual']]
tests=r'''
static void test_wheel_adapter(int action,int delta,int x,int y){(void)x;(void)y;test_wheel(action,delta);}
static void message_wheel(int delta,WORD mods){
 WPARAM wParam=((WPARAM)(WORD)delta<<16)|mods;
 LPARAM lParam=0;
'''
end=r'''
}
static void reset(int index,int slow){
 _viv_directory_step_count=0;_viv_navigation_in_flight=0;_viv_wheel_delta=0;
 delay_directory=0;delay_image=slow;opens=zoom_events=0;last_action=-1;_viv_active_load=NULL;
 select_file(&current,index);requested=shown=current;
}
int main(void){
 int delayed,i;
 for(delayed=0;delayed<2;delayed++){
  reset(100,delayed);
  for(i=0;i<10;i++)message_wheel(-120,0);
  assert(index_of(&current)==110);
  for(i=0;i<5;i++)message_wheel(120,0);
  assert(index_of(&current)==105);
  reset(100,delayed);
  message_wheel(-120,0);message_wheel(120,0);message_wheel(120,0);
  assert(index_of(&current)==99);
  message_wheel(-120,MK_CONTROL);assert(index_of(&current)==99&&zoom_events==1);
 }
 reset(100,1);
 for(i=0;i<4;i++)message_wheel(-30,0);
 assert(index_of(&current)==101);
 message_wheel(60,0);message_wheel(120,MK_CONTROL);message_wheel(60,0);
 assert(index_of(&current)==101);
 message_wheel(60,0);assert(index_of(&current)==100);
 reset(1,1);delay_directory=1;
 for(i=0;i<1000;i++)message_wheel(-120,0);
 assert(_viv_directory_step_count==1000);
 delay_directory=0;_viv_replay_directory_navigation(TRUE);
 assert(index_of(&current)==1001);
 reset(1,0);message_wheel(120,0);assert(index_of(&current)==1);
 reset(2000,0);message_wheel(-120,0);assert(index_of(&current)==2000);
 free(_viv_directory_steps);
 puts("FYTO_NAVIGATION_LOGICAL_TARGET_MODIFIERS_HIGHRES_1000_EVENTS_VERIFIED");
 return 0;
}
'''
out=root/'build/navigation-under-test.c'
out.parent.mkdir(exist_ok=True)
out.write_text(preamble+'\n'.join(parts)+'\n'+wheel+'\n'+tests+case+end,encoding='utf-8')
print('Navigation harness uses production input branch and navigation functions.')
