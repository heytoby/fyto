/* Read-only, cancellable file stream. Clones keep independent cursors on the
   same open file, including after rename/replacement. No compressed-file copy. */
#ifndef FYTO_FILE_STREAM_H
#define FYTO_FILE_STREAM_H
typedef struct {
    HANDLE file;
    LONG refs;
    CRITICAL_SECTION lock;
    ULONGLONG size;
    FILETIME created,accessed,written;
    BOOL (*cancelled)(void *);
    void *context;
} fyto_file_backing_t;
typedef struct {
    IStream stream;
    LONG refs;
    fyto_file_backing_t *backing;
    ULONGLONG position;
} fyto_file_stream_t;
static IStream *fyto_file_stream_cursor(fyto_file_backing_t *,ULONGLONG);
static BOOL fyto_file_stream_stopped(fyto_file_backing_t *b)
{
    return b->cancelled && b->cancelled(b->context);
}
static HRESULT STDMETHODCALLTYPE fyto_file_query(IStream *self,REFIID iid,void **out)
{
    if(!out)return E_POINTER;*out=NULL;
    if(!IsEqualIID(iid,&IID_IUnknown) && !IsEqualIID(iid,&IID_ISequentialStream) &&
        !IsEqualIID(iid,&IID_IStream))return E_NOINTERFACE;
    self->lpVtbl->AddRef(self);*out=self;return S_OK;
}
static ULONG STDMETHODCALLTYPE fyto_file_addref(IStream *self)
{
    return InterlockedIncrement(&((fyto_file_stream_t *)self)->refs);
}
static ULONG STDMETHODCALLTYPE fyto_file_release(IStream *self)
{
    fyto_file_stream_t *s=(fyto_file_stream_t *)self;
    ULONG refs=InterlockedDecrement(&s->refs);
    if(!refs){
        fyto_file_backing_t *b=s->backing;
        HeapFree(GetProcessHeap(),0,s);
        if(!InterlockedDecrement(&b->refs)){
            CloseHandle(b->file);DeleteCriticalSection(&b->lock);
            HeapFree(GetProcessHeap(),0,b);
        }
    }
    return refs;
}
static HRESULT STDMETHODCALLTYPE fyto_file_read(IStream *self,void *bytes,ULONG size,ULONG *read)
{
    fyto_file_stream_t *s=(fyto_file_stream_t *)self;fyto_file_backing_t *b=s->backing;
    ULONG total=0;HRESULT result=S_OK;
    if(read)*read=0;
    if(size && !bytes)return STG_E_INVALIDPOINTER;
    EnterCriticalSection(&b->lock);
    while(total<size && s->position<b->size){
        LARGE_INTEGER offset;DWORD got=0,count=min(size-total,65536u);
        if(fyto_file_stream_stopped(b)){result=HRESULT_FROM_WIN32(ERROR_CANCELLED);break;}
        if((ULONGLONG)count>b->size-s->position)count=(DWORD)(b->size-s->position);
        offset.QuadPart=(LONGLONG)s->position;
        if(!SetFilePointerEx(b->file,offset,NULL,FILE_BEGIN) ||
            !ReadFile(b->file,(BYTE *)bytes+total,count,&got,NULL)){
            result=HRESULT_FROM_WIN32(GetLastError());break;
        }
        if(!got)break;
        total+=got;s->position+=got;
    }
    LeaveCriticalSection(&b->lock);
    if(read)*read=total;
    return FAILED(result)?result:(total==size?S_OK:S_FALSE);
}
static HRESULT STDMETHODCALLTYPE fyto_file_write(IStream *self,const void *bytes,ULONG size,ULONG *written)
{
    (void)self;(void)bytes;(void)size;if(written)*written=0;return STG_E_ACCESSDENIED;
}
static HRESULT STDMETHODCALLTYPE fyto_file_seek(IStream *self,LARGE_INTEGER delta,DWORD origin,ULARGE_INTEGER *position)
{
    fyto_file_stream_t *s=(fyto_file_stream_t *)self;fyto_file_backing_t *b=s->backing;
    LONGLONG base;HRESULT result=S_OK;
    EnterCriticalSection(&b->lock);
    if(origin==STREAM_SEEK_SET)base=0;
    else if(origin==STREAM_SEEK_CUR)base=(LONGLONG)s->position;
    else if(origin==STREAM_SEEK_END)base=(LONGLONG)b->size;
    else {result=STG_E_INVALIDFUNCTION;goto done;}
    if((delta.QuadPart<0 && delta.QuadPart < -base) ||
        (delta.QuadPart>0 && base>MAXLONGLONG-delta.QuadPart)){
        result=STG_E_INVALIDFUNCTION;goto done;
    }
    s->position=(ULONGLONG)(base+delta.QuadPart);
    if(position)position->QuadPart=s->position;
done:
    LeaveCriticalSection(&b->lock);return result;
}
static HRESULT STDMETHODCALLTYPE fyto_file_size(IStream *self,ULARGE_INTEGER size)
{
    (void)self;(void)size;return STG_E_ACCESSDENIED;
}
static HRESULT STDMETHODCALLTYPE fyto_file_copy(IStream *self,IStream *out,ULARGE_INTEGER size,
    ULARGE_INTEGER *read,ULARGE_INTEGER *written)
{
    BYTE buffer[65536];ULONGLONG count=0,committed=0;HRESULT result=S_OK;
    if(read)read->QuadPart=0;if(written)written->QuadPart=0;
    if(!out)return STG_E_INVALIDPOINTER;
    while(count<size.QuadPart){
        ULONG got=0,put=0,request=(ULONG)min(size.QuadPart-count,(ULONGLONG)sizeof(buffer));
        HRESULT status=self->lpVtbl->Read(self,buffer,request,&got);
        count+=got;
        if(got){
            result=out->lpVtbl->Write(out,buffer,got,&put);committed+=put;
            if(FAILED(result))break;
            if(put!=got){result=STG_E_MEDIUMFULL;break;}
        }
        if(FAILED(status)){result=status;break;}
        if(!got || status==S_FALSE)break;
    }
    if(read)read->QuadPart=count;if(written)written->QuadPart=committed;
    return SUCCEEDED(result) && count<size.QuadPart ? S_FALSE : result;
}
static HRESULT STDMETHODCALLTYPE fyto_file_commit(IStream *self,DWORD flags)
{
    (void)self;(void)flags;return S_OK;
}
static HRESULT STDMETHODCALLTYPE fyto_file_revert(IStream *self)
{
    (void)self;return STG_E_INVALIDFUNCTION;
}
static HRESULT STDMETHODCALLTYPE fyto_file_region(IStream *self,ULARGE_INTEGER offset,ULARGE_INTEGER size,DWORD type)
{
    (void)self;(void)offset;(void)size;(void)type;return STG_E_INVALIDFUNCTION;
}
static HRESULT STDMETHODCALLTYPE fyto_file_stat(IStream *self,STATSTG *stat,DWORD flags)
{
    fyto_file_backing_t *b=((fyto_file_stream_t *)self)->backing;
    (void)flags;if(!stat)return STG_E_INVALIDPOINTER;
    ZeroMemory(stat,sizeof(*stat));stat->type=STGTY_STREAM;stat->cbSize.QuadPart=b->size;
    stat->ctime=b->created;stat->atime=b->accessed;stat->mtime=b->written;
    stat->grfMode=STGM_READ|STGM_SHARE_DENY_NONE;return S_OK;
}
static HRESULT STDMETHODCALLTYPE fyto_file_clone(IStream *self,IStream **out)
{
    fyto_file_stream_t *s=(fyto_file_stream_t *)self;
    if(!out)return E_POINTER;*out=NULL;
    EnterCriticalSection(&s->backing->lock);
    *out=fyto_file_stream_cursor(s->backing,s->position);
    LeaveCriticalSection(&s->backing->lock);
    return *out?S_OK:E_OUTOFMEMORY;
}
static IStream *fyto_file_stream_cursor(fyto_file_backing_t *backing,ULONGLONG position)
{
    static IStreamVtbl vtable={
        fyto_file_query,fyto_file_addref,fyto_file_release,fyto_file_read,fyto_file_write,
        fyto_file_seek,fyto_file_size,fyto_file_copy,fyto_file_commit,fyto_file_revert,
        fyto_file_region,fyto_file_region,fyto_file_stat,fyto_file_clone
    };
    fyto_file_stream_t *s=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*s));
    if(!s)return NULL;
    s->stream.lpVtbl=&vtable;s->refs=1;s->backing=backing;s->position=position;
    InterlockedIncrement(&backing->refs);return &s->stream;
}
/* Ownership of file transfers only on success. */
static IStream *fyto_file_stream_take(HANDLE file,BOOL (*cancelled)(void *),void *context)
{
    fyto_file_backing_t *b;BY_HANDLE_FILE_INFORMATION info;LARGE_INTEGER size;IStream *stream;
    if((cancelled && cancelled(context)) || file==INVALID_HANDLE_VALUE || !GetFileSizeEx(file,&size) || size.QuadPart<0 ||
        !GetFileInformationByHandle(file,&info))return NULL;
    b=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*b));if(!b)return NULL;
    InitializeCriticalSection(&b->lock);
    b->file=file;b->size=(ULONGLONG)size.QuadPart;b->cancelled=cancelled;b->context=context;
    b->created=info.ftCreationTime;b->accessed=info.ftLastAccessTime;b->written=info.ftLastWriteTime;
    stream=fyto_file_stream_cursor(b,0);
    if(!stream){DeleteCriticalSection(&b->lock);HeapFree(GetProcessHeap(),0,b);}
    return stream;
}
#endif
