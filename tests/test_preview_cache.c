#define UNICODE
#include <windows.h>
#include <wchar.h>
#include <stdio.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
typedef struct {HBITMAP hbitmap;int wide,high;} _viv_preview_t;
static struct {WIN32_FIND_DATA file;_viv_preview_t image;unsigned used;} _viv_recent_previews[8];
static unsigned _viv_recent_preview_slot;
static WIN32_FIND_DATA file,*_viv_frame_fd=&file;
#define FYTO_CACHE_TOTAL_BYTES 1000000
static int string_compare(const wchar_t *a,const wchar_t *b){return wcscmp(a,b);}
static BOOL _viv_same_image(const WIN32_FIND_DATA *a,const WIN32_FIND_DATA *b){return !wcscmp(a->cFileName,b->cFileName);}
static HBITMAP _viv_copy_bitmap(HBITMAP bitmap,int w,int h){return CreateBitmap(w,h,1,32,NULL);}
static unsigned long long _viv_cache_footprint(void){return 0;}
static BOOL _viv_cache_evict_one(BOOL frames){assert(0);return FALSE;}
#include "../build/cache-under-test.h"
int main(void){
    int i,n;DWORD before=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
    for(i=0;i<8;i++){
        _viv_preview_t p={CreateBitmap(1,1,1,32,NULL),1,1};assert(p.hbitmap);
        swprintf_s(file.cFileName,MAX_PATH,L"%d",i);_viv_remember_preview(p);
    }
    for(n=0;n<1000;n++){
        _viv_preview_t p={CreateBitmap(1,1,1,32,NULL),1,1};assert(p.hbitmap);
        swprintf_s(file.cFileName,MAX_PATH,L"%d",n%2+3);_viv_remember_preview(p);
        for(i=0;i<8;i++){
            _viv_preview_t cached={0};swprintf_s(file.cFileName,MAX_PATH,L"%d",i);
            _viv_take_recent_preview(&file,&cached);assert(cached.hbitmap);DeleteObject(cached.hbitmap);
        }
    }
    for(i=0;i<8;i++)DeleteObject(_viv_recent_previews[i].image.hbitmap);
    assert(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)==before);
    puts("8 distinct images retained across 1000 duplicate visits; 8000 hits; stable GDI handles");return 0;
}
