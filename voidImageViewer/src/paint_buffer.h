/* A private surface for committing complete GDI paints in one blit. */
#ifndef FYTO_PAINT_BUFFER_H
#define FYTO_PAINT_BUFFER_H

typedef struct {
    HDC dc;
    HBITMAP bitmap;
    HGDIOBJ original;
    int width, height;
} fyto_paint_buffer_t;

static void fyto_paint_buffer_release(fyto_paint_buffer_t *buffer)
{
    if (buffer->dc) {
        SelectObject(buffer->dc, buffer->original);
        DeleteObject(buffer->bitmap);
        DeleteDC(buffer->dc);
    }
    ZeroMemory(buffer, sizeof(*buffer));
}

static HDC fyto_paint_buffer_dc(fyto_paint_buffer_t *buffer, HDC target, int width, int height)
{
    fyto_paint_buffer_t replacement = {0};
    if (width <= 0 || height <= 0 || (UINT64)width * height * 4 > 128ull * 1024 * 1024)
        return NULL;
    if (os_GetLayout && (os_GetLayout(target) & LAYOUT_RTL)) return NULL;
    if (buffer->dc && width <= buffer->width && height <= buffer->height) return buffer->dc;

    // Keep enough room for both portrait and landscape window sizes.
    if ((UINT64)max(width, buffer->width) * max(height, buffer->height) * 4 <= 128ull * 1024 * 1024)
    {
        width = max(width, buffer->width);
        height = max(height, buffer->height);
    }
    replacement.dc = CreateCompatibleDC(target);
    if (!replacement.dc) return NULL;
    replacement.bitmap = CreateCompatibleBitmap(target, width, height);
    if (replacement.bitmap)
        replacement.original = SelectObject(replacement.dc, replacement.bitmap);
    if (!replacement.original || replacement.original == HGDI_ERROR) {
        if (replacement.bitmap) DeleteObject(replacement.bitmap);
        DeleteDC(replacement.dc);
        return NULL;
    }

    replacement.width = width;
    replacement.height = height;
    fyto_paint_buffer_release(buffer);
    *buffer = replacement;
    return buffer->dc;
}
#endif

