/* Read-only wrapper for interrupting obsolete JPEG decode work. */
#ifndef FYTO_JPEG_STREAM_H
#define FYTO_JPEG_STREAM_H

typedef struct {
    IStream stream;
    LONG refs;
    IStream *source;
    BOOL (*cancelled)(void *);
    void *context;
} fyto_jpeg_stream_t;

static IStream *fyto_jpeg_stream_create(IStream *source, BOOL (*cancelled)(void *), void *context);

static HRESULT STDMETHODCALLTYPE fyto_jpeg_stream_query(IStream *self, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualIID(iid, &IID_IUnknown) && !IsEqualIID(iid, &IID_ISequentialStream) &&
        !IsEqualIID(iid, &IID_IStream)) return E_NOINTERFACE;
    self->lpVtbl->AddRef(self);
    *out = self;
    return S_OK;
}

static ULONG STDMETHODCALLTYPE fyto_jpeg_stream_addref(IStream *self)
{
    fyto_jpeg_stream_t *stream = (fyto_jpeg_stream_t *)self;
    return InterlockedIncrement(&stream->refs);
}

static ULONG STDMETHODCALLTYPE fyto_jpeg_stream_release(IStream *self)
{
    fyto_jpeg_stream_t *stream = (fyto_jpeg_stream_t *)self;
    ULONG refs = InterlockedDecrement(&stream->refs);
    if (!refs) {
        stream->source->lpVtbl->Release(stream->source);
        HeapFree(GetProcessHeap(), 0, stream);
    }
    return refs;
}

static HRESULT STDMETHODCALLTYPE fyto_jpeg_stream_read(IStream *self, void *bytes, ULONG size, ULONG *read)
{
    fyto_jpeg_stream_t *stream = (fyto_jpeg_stream_t *)self;
    if (stream->cancelled(stream->context)) {
        if (read) *read = 0;
        return HRESULT_FROM_WIN32(ERROR_CANCELLED);
    }
    return stream->source->lpVtbl->Read(stream->source, bytes, size, read);
}

static HRESULT STDMETHODCALLTYPE fyto_jpeg_stream_write(IStream *self, const void *bytes, ULONG size, ULONG *written)
{
    (void)self; (void)bytes; (void)size;
    if (written) *written = 0;
    return STG_E_ACCESSDENIED;
}

static HRESULT STDMETHODCALLTYPE fyto_jpeg_stream_seek(IStream *self, LARGE_INTEGER offset, DWORD origin, ULARGE_INTEGER *pos)
{
    fyto_jpeg_stream_t *stream = (fyto_jpeg_stream_t *)self;
    return stream->source->lpVtbl->Seek(stream->source, offset, origin, pos);
}

static HRESULT STDMETHODCALLTYPE fyto_jpeg_stream_size(IStream *self, ULARGE_INTEGER size)
{
    (void)self; (void)size;
    return STG_E_ACCESSDENIED;
}

static HRESULT STDMETHODCALLTYPE fyto_jpeg_stream_copy(IStream *self, IStream *destination, ULARGE_INTEGER size,
    ULARGE_INTEGER *read, ULARGE_INTEGER *written)
{
    fyto_jpeg_stream_t *stream = (fyto_jpeg_stream_t *)self;
    if (stream->cancelled(stream->context)) {
        if (read) read->QuadPart = 0;
        if (written) written->QuadPart = 0;
        return HRESULT_FROM_WIN32(ERROR_CANCELLED);
    }
    return stream->source->lpVtbl->CopyTo(stream->source, destination, size, read, written);
}

static HRESULT STDMETHODCALLTYPE fyto_jpeg_stream_commit(IStream *self, DWORD flags)
{
    (void)self; (void)flags;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE fyto_jpeg_stream_revert(IStream *self)
{
    (void)self;
    return STG_E_INVALIDFUNCTION;
}

static HRESULT STDMETHODCALLTYPE fyto_jpeg_stream_region(IStream *self, ULARGE_INTEGER offset, ULARGE_INTEGER size, DWORD type)
{
    (void)self; (void)offset; (void)size; (void)type;
    return STG_E_INVALIDFUNCTION;
}

static HRESULT STDMETHODCALLTYPE fyto_jpeg_stream_stat(IStream *self, STATSTG *stat, DWORD flags)
{
    fyto_jpeg_stream_t *stream = (fyto_jpeg_stream_t *)self;
    return stream->source->lpVtbl->Stat(stream->source, stat, flags);
}

static HRESULT STDMETHODCALLTYPE fyto_jpeg_stream_clone(IStream *self, IStream **out)
{
    fyto_jpeg_stream_t *stream = (fyto_jpeg_stream_t *)self;
    IStream *copy;
    HRESULT result;
    if (!out) return E_POINTER;
    *out = NULL;
    result = stream->source->lpVtbl->Clone(stream->source, &copy);
    if (FAILED(result)) return result;
    *out = fyto_jpeg_stream_create(copy, stream->cancelled, stream->context);
    copy->lpVtbl->Release(copy);
    return *out ? S_OK : E_OUTOFMEMORY;
}

static IStream *fyto_jpeg_stream_create(IStream *source, BOOL (*cancelled)(void *), void *context)
{
    static IStreamVtbl vtable = {
        fyto_jpeg_stream_query, fyto_jpeg_stream_addref, fyto_jpeg_stream_release,
        fyto_jpeg_stream_read, fyto_jpeg_stream_write, fyto_jpeg_stream_seek,
        fyto_jpeg_stream_size, fyto_jpeg_stream_copy, fyto_jpeg_stream_commit,
        fyto_jpeg_stream_revert, fyto_jpeg_stream_region, fyto_jpeg_stream_region,
        fyto_jpeg_stream_stat, fyto_jpeg_stream_clone
    };
    fyto_jpeg_stream_t *stream = HeapAlloc(GetProcessHeap(), 0, sizeof(*stream));
    if (!stream) return NULL;
    stream->stream.lpVtbl = &vtable;
    stream->refs = 1;
    stream->source = source;
    stream->cancelled = cancelled;
    stream->context = context;
    source->lpVtbl->AddRef(source);
    return &stream->stream;
}
#endif

