/* Decode a JPEG through fyto's real fast path and dump raw BGRX.
 * Used to validate the ICC matrix/TRC conversion against lcms. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include "../voidImageViewer/src/fast_jpeg.h"

static BYTE *read_file(const char *path, DWORD *size)
{
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    BYTE *buf;
    DWORD done = 0;
    if (h == INVALID_HANDLE_VALUE) return NULL;
    *size = GetFileSize(h, NULL);
    buf = (BYTE *)malloc(*size);
    if (buf) ReadFile(h, buf, *size, &done, NULL);
    CloseHandle(h);
    if (!buf || done != *size) { free(buf); return NULL; }
    return buf;
}

int main(int argc, char **argv)
{
    BYTE *data;
    DWORD size = 0, t0, ms;
    HGLOBAL global;
    IStream *stream = NULL;
    fyto_wic_image jpeg;
    HBITMAP bitmap;
    UINT wide = 0, high = 0;
    BITMAP dib;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (argc < 3) { printf("usage: test_icc <in.jpg> <out.raw>\n"); return 1; }
    data = read_file(argv[1], &size);
    if (!data) { printf("read failed\n"); return 2; }

    global = GlobalAlloc(GMEM_MOVEABLE, size);
    memcpy(GlobalLock(global), data, size);
    GlobalUnlock(global);
    if (FAILED(CreateStreamOnHGlobal(global, TRUE, &stream))) { printf("stream failed\n"); return 3; }

    ZeroMemory(&jpeg, sizeof(jpeg));
    fyto_jpeg_set_input(&jpeg, stream, NULL, NULL);
    if (!fyto_jpeg_open(&jpeg, stream, 1)) { printf("open failed (no turbo or bad file)\n"); return 4; }
    printf("icc state=%d (%s)\n", jpeg.icc.state,
        jpeg.icc.state == FYTO_ICC_NONE ? "none" :
        jpeg.icc.state == FYTO_ICC_IDENTITY ? "identity" :
        jpeg.icc.state == FYTO_ICC_MATRIX ? "matrix" : "unsupported");

    /* "wic" forces the pre-1.0.0.53 path (WIC decode + IWICColorTransform). */
    if (argc > 3 && !strcmp(argv[3], "wic")) jpeg.icc.state = FYTO_ICC_UNSUPPORTED;
    t0 = GetTickCount();
    bitmap = fyto_jpeg_full(&jpeg, 0, &wide, &high);
    ms = GetTickCount() - t0;
    if (!bitmap) { printf("decode failed\n"); return 5; }
    if (GetObject(bitmap, sizeof(dib), &dib) != sizeof(dib)) { printf("GetObject failed\n"); return 6; }

    {
        FILE *f = fopen(argv[2], "wb");
        UINT y;
        const BYTE *bits = (const BYTE *)dib.bmBits;
        for (y = 0; y < high; y++) fwrite(bits + (SIZE_T)y * dib.bmWidthBytes, 1, (SIZE_T)wide * 4, f);
        fclose(f);
        printf("wrote %s %ux%u in %lu ms\n", argv[2], wide, high, ms);
    }
    fyto_wic_close(&jpeg);
    DeleteObject(bitmap);
    stream->lpVtbl->Release(stream);
    free(data);
    return 0;
}
