#define COBJMACROS
#define UNICODE
#include <windows.h>
#include <objbase.h>
#include <stdio.h>
#include <wchar.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include "../voidImageViewer/src/file_stream.h"
typedef struct {int calls,after;} cancel_t;
static BOOL cancelled(void *p){cancel_t *c=p;return ++c->calls>=c->after && c->after>0;}
static void seek(IStream *s,LONGLONG offset,DWORD origin){LARGE_INTEGER n;n.QuadPart=offset;assert(SUCCEEDED(s->lpVtbl->Seek(s,n,origin,NULL)));}
int wmain(void){
    wchar_t temp[MAX_PATH],path[MAX_PATH],renamed[MAX_PATH];static BYTE input[1048576],output[131072];
    HANDLE file;DWORD got,before,after;IStream *s,*clone=NULL,*copy=NULL;cancel_t stop={0};
    ULONG n;int i;ULARGE_INTEGER request,read,written;STATSTG stat;HRESULT hr;
    for(i=0;i<sizeof(input);i++)input[i]=(BYTE)i;
    assert(GetTempPathW(MAX_PATH,temp));assert(GetTempFileNameW(temp,L"fyt",0,path));
    swprintf_s(renamed,MAX_PATH,L"%s.renamed",path);
    file=CreateFileW(path,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,CREATE_ALWAYS,0,NULL);
    assert(file!=INVALID_HANDLE_VALUE);assert(WriteFile(file,input,sizeof(input),&got,NULL)&&got==sizeof(input));
    s=fyto_file_stream_take(file,cancelled,&stop);assert(s);
    assert(SUCCEEDED(s->lpVtbl->Stat(s,&stat,STATFLAG_NONAME))&&stat.cbSize.QuadPart==sizeof(input));
    assert(SUCCEEDED(s->lpVtbl->Read(s,output,3,&n))&&n==3&&output[2]==2);
    assert(SUCCEEDED(s->lpVtbl->Clone(s,&clone)));
    seek(clone,70,STREAM_SEEK_SET);assert(SUCCEEDED(clone->lpVtbl->Read(clone,output,2,&n))&&output[0]==70);
    assert(SUCCEEDED(s->lpVtbl->Read(s,output,2,&n))&&output[0]==3);
    assert(MoveFileExW(path,renamed,0));assert(DeleteFileW(renamed));
    seek(s,-3,STREAM_SEEK_END);assert(s->lpVtbl->Read(s,output,10,&n)==S_FALSE&&n==3&&output[0]==253);
    assert(s->lpVtbl->Read(s,output,1,&n)==S_FALSE&&n==0);
    seek(s,0,STREAM_SEEK_SET);assert(SUCCEEDED(CreateStreamOnHGlobal(NULL,TRUE,&copy)));
    request.QuadPart=sizeof(input);
    assert(SUCCEEDED(s->lpVtbl->CopyTo(s,copy,request,&read,&written))&&read.QuadPart==sizeof(input)&&written.QuadPart==sizeof(input));
    seek(s,0,STREAM_SEEK_SET);request.QuadPart=sizeof(input)+1;
    assert(s->lpVtbl->CopyTo(s,copy,request,&read,&written)==S_FALSE && read.QuadPart==sizeof(input));
    seek(copy,700,STREAM_SEEK_SET);assert(SUCCEEDED(copy->lpVtbl->Read(copy,output,16,&n))&&output[0]==(BYTE)700);
    copy->lpVtbl->Release(copy);
    seek(s,0,STREAM_SEEK_SET);stop.after=stop.calls+2;
    hr=s->lpVtbl->Read(s,output,sizeof(output),&n);
    assert(hr==HRESULT_FROM_WIN32(ERROR_CANCELLED)&&n==65536);
    stop.after=0;
    assert(GetProcessHandleCount(GetCurrentProcess(),&before));
    for(i=0;i<2000;i++){IStream *item=NULL;assert(SUCCEEDED(s->lpVtbl->Clone(s,&item)));seek(item,i,STREAM_SEEK_SET);assert(SUCCEEDED(item->lpVtbl->Read(item,output,1,&n))&&output[0]==(BYTE)i);item->lpVtbl->Release(item);}
    assert(GetProcessHandleCount(GetCurrentProcess(),&after)&&before==after);
    clone->lpVtbl->Release(clone);s->lpVtbl->Release(s);
    assert(GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES&&GetFileAttributesW(renamed)==INVALID_FILE_ATTRIBUTES);
    puts("File-stream read/seek/EOF/copy, independent cursors, rename/delete sharing, chunk cancellation and 2000 clone lifecycles passed");
    return 0;
}
