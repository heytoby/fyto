/* The AVX instructions stay in this translation unit. */
#include <windows.h>

#define WUFFS_IMPLEMENTATION
#define WUFFS_CONFIG__STATIC_FUNCTIONS
#define WUFFS_CONFIG__MODULES
#define WUFFS_CONFIG__MODULE__BASE
#define WUFFS_CONFIG__MODULE__ADLER32
#define WUFFS_CONFIG__MODULE__CRC32
#define WUFFS_CONFIG__MODULE__DEFLATE
#define WUFFS_CONFIG__MODULE__ZLIB
#define WUFFS_CONFIG__MODULE__PNG
#include "../wuffs/wuffs-v0.3.c"

BOOL fyto_decode_png_avx(BYTE *data, DWORD size, BYTE *pixels, UINT wide, UINT high, BOOL alpha,
    BOOL (*cancelled)(void *), void *context)
{
    wuffs_png__decoder *decoder;
    wuffs_base__io_buffer input;
    wuffs_base__image_config config;
    wuffs_base__pixel_buffer output;
    wuffs_base__status status;
    uint64_t work_size;
    BYTE *work = NULL;
    BOOL decoded = FALSE;

    decoder = wuffs_png__decoder__alloc();
    if (!decoder) return FALSE;
    input = wuffs_base__ptr_u8__reader(data, size, true);
    status = wuffs_png__decoder__decode_image_config(decoder, &config, &input);
    if (status.repr || cancelled(context) ||
        wuffs_base__pixel_config__width(&config.pixcfg) != wide ||
        wuffs_base__pixel_config__height(&config.pixcfg) != high) goto done;
    wuffs_base__pixel_config__set(&config.pixcfg,
        alpha ? WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL : WUFFS_BASE__PIXEL_FORMAT__BGRX,
        WUFFS_BASE__PIXEL_SUBSAMPLING__NONE, wide, high);
    status = wuffs_base__pixel_buffer__set_from_slice(&output, &config.pixcfg,
        wuffs_base__make_slice_u8(pixels, (size_t)wide * high * 4));
    if (status.repr) goto done;
    work_size = wuffs_png__decoder__workbuf_len(decoder).max_incl;
    if (work_size > SIZE_MAX) goto done;
    work = malloc((size_t)work_size);
    if (!work) goto done;
    input.meta.wi = input.meta.ri;
    do
    {
        if (cancelled(context)) goto done;
        input.meta.wi += min((size_t)size - input.meta.wi, 256 * 1024);
        input.meta.closed = input.meta.wi == size;
        status = wuffs_png__decoder__decode_frame(decoder, &output, &input, WUFFS_BASE__PIXEL_BLEND__SRC,
            wuffs_base__make_slice_u8(work, (size_t)work_size), NULL);
    }
    while (status.repr == wuffs_base__suspension__short_read && !input.meta.closed);
    decoded = !status.repr && !cancelled(context);

done:
    free(work);
    free(decoder);
    return decoded;
}
