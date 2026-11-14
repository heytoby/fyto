#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <propidl.h>
#include <limits.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include "../voidImageViewer/src/fast_jpeg.h"

static BOOL cancel_decode(void *context)
{
    int *calls = context;
    return ++*calls >= 6;
}
static int compare(HBITMAP a, HBITMAP b)
{
    BITMAP x, y;
    UINT row, col, channels;
    double total = 0;
    unsigned largest = 0;
    if (!a || !b || !GetObject(a, sizeof(x), &x) || !GetObject(b, sizeof(y), &y)) return 0;
    if (x.bmWidth != y.bmWidth || x.bmHeight != y.bmHeight || x.bmBitsPixel != y.bmBitsPixel) return 0;
    channels = x.bmBitsPixel / 8;
    for (row = 0; row < (UINT)x.bmHeight; ++row)
        for (col = 0; col < (UINT)x.bmWidth; ++col)
        {
            UINT k;
            for (k = 0; k < 3; ++k)
            {
                int delta = abs(((BYTE *)x.bmBits)[row*x.bmWidthBytes+col*channels+k] -
                                ((BYTE *)y.bmBits)[row*y.bmWidthBytes+col*channels+k]);
                total += delta;
                if ((UINT)delta > largest) largest = delta;
            }
        }
    total /= (double)x.bmWidth * x.bmHeight * 3;
    printf("PIXEL_DIFF mean=%.5f max=%u\n", total, largest);
    return total < 2.0 && largest <= 40;
}
int wmain(int argc, wchar_t **argv)
{
    int i, failed = 0;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    for (i = 1; i < argc; ++i)
    {
        HANDLE file = CreateFileW(argv[i], GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        DWORD size, got;
        HGLOBAL data;
        IStream *stream = NULL;
        fyto_wic_image jpeg = {0};
        BYTE *bytes;
        const BYTE *input;
        HBITMAP fast = NULL, reference = NULL;
        UINT w, h;
        int orientation, calls = 0;
        BOOL ok;
        if (file == INVALID_HANDLE_VALUE) return 2;
        size = GetFileSize(file, NULL);
        data = GlobalAlloc(GMEM_MOVEABLE, size);
        bytes = GlobalLock(data);
        ok = bytes && ReadFile(file, bytes, size, &got, NULL) && got == size;
        GlobalUnlock(data);
        CloseHandle(file);
        if (!ok || FAILED(CreateStreamOnHGlobal(data, TRUE, &stream))) return 3;
        fyto_jpeg_set_input(&jpeg, stream, NULL, NULL);
        if (!fyto_jpeg_open(&jpeg, stream, TRUE)) return 4;
        input = jpeg.input_data;
        orientation = fyto_jpeg_orientation(&jpeg);
        fast = fyto_jpeg_full(&jpeg, orientation, &w, &h);
        jpeg.input_data = NULL;
        reference = fyto_jpeg_full(&jpeg, orientation, &w, &h);
        if (!compare(fast, reference)) failed = 1;
        if (fast) DeleteObject(fast);
        if (reference) DeleteObject(reference);
        jpeg.input_data = input;
        fast = fyto_jpeg_preview(&jpeg, 1024, &w, &h);
        jpeg.input_data = NULL;
        reference = fyto_jpeg_preview(&jpeg, 1024, &w, &h);
        if (!compare(fast, reference)) failed = 1;
        if (fast) DeleteObject(fast);
        if (reference) DeleteObject(reference);
        fast = fyto_jpeg_decode_pixels(input, jpeg.input_size, jpeg.wide, jpeg.high, 1, 32, &w, &h, cancel_decode, &calls);
        if (fast || calls < 6) failed = 1;
        if (fast) DeleteObject(fast);
        fast = fyto_jpeg_decode_pixels(input, min(32, jpeg.input_size), jpeg.wide, jpeg.high, 1, 32, &w, &h, NULL, NULL);
        if (fast) { DeleteObject(fast); failed = 1; }
        printf("JPEG_CASE %ls orientation=%d cancelled=%d result=%d\n", argv[i], orientation, calls, failed);
        fyto_wic_close(&jpeg);
        IStream_Release(stream);
    }
    CoUninitialize();
    return failed;
}
