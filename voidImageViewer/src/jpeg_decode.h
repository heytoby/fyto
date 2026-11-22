/* Cancellable libjpeg-turbo decoding for fyto. See LICENSE. */
#ifndef FYTO_JPEG_DECODE_H
#define FYTO_JPEG_DECODE_H
#include <stdio.h>
#include <setjmp.h>
#include <jpeglib.h>

typedef struct fyto_jpeg_decode {
    struct jpeg_decompress_struct codec;
    struct jpeg_error_mgr error;
    struct jpeg_source_mgr input;
    jmp_buf jump;
    IStream *stream;
    BYTE buffer[65536];
    const BYTE *bytes;
    size_t size, position;
    BOOL (*cancelled)(void *);
    void *context;
    HBITMAP bitmap;
} fyto_jpeg_decode;

static void fyto_jpeg_error(j_common_ptr codec)
{
    fyto_jpeg_decode *decode = (fyto_jpeg_decode *)codec->client_data;
    longjmp(decode->jump, 1);
}
static void fyto_jpeg_silent(j_common_ptr codec) { (void)codec; }
static void fyto_jpeg_source_start(j_decompress_ptr codec) { (void)codec; }
static void fyto_jpeg_source_end(j_decompress_ptr codec) { (void)codec; }
static boolean fyto_jpeg_source_fill(j_decompress_ptr codec)
{
    fyto_jpeg_decode *decode = (fyto_jpeg_decode *)codec->client_data;
    size_t count;
    if (decode->cancelled && decode->cancelled(decode->context))
        longjmp(decode->jump, 1);
    if (decode->stream)
    {
        ULONG read = 0;
        if (FAILED(decode->stream->lpVtbl->Read(decode->stream,
            decode->buffer, sizeof(decode->buffer), &read)) || !read)
            longjmp(decode->jump, 1);
        count = read;
        decode->input.next_input_byte = decode->buffer;
    }
    else
    {
        count = decode->size - decode->position;
        if (!count) longjmp(decode->jump, 1);
        if (count > sizeof(decode->buffer)) count = sizeof(decode->buffer);
        decode->input.next_input_byte = decode->bytes + decode->position;
        decode->position += count;
    }
    decode->input.bytes_in_buffer = count;
    return TRUE;
}
static void fyto_jpeg_source_skip(j_decompress_ptr codec, long count)
{
    if (count <= 0) return;
    while ((size_t)count > codec->src->bytes_in_buffer)
    {
        count -= (long)codec->src->bytes_in_buffer;
        fyto_jpeg_source_fill(codec);
    }
    codec->src->next_input_byte += count;
    codec->src->bytes_in_buffer -= count;
}

static HBITMAP fyto_jpeg_decode_input(IStream *stream, const BYTE *bytes, size_t size,
    UINT expected_wide, UINT expected_high, UINT denominator, UINT bit_count,
    UINT *wide, UINT *high, BOOL (*cancelled)(void *), void *context)
{
    fyto_jpeg_decode *decode;
    HBITMAP result = NULL;
    BYTE *pixels;
    UINT stride;
    if ((!stream && (!bytes || !size)) || (bit_count != 24 && bit_count != 32)) return NULL;
    if (stream)
    {
        LARGE_INTEGER start = {0};
        if (FAILED(stream->lpVtbl->Seek(stream, start, STREAM_SEEK_SET, NULL))) return NULL;
    }
    decode = (fyto_jpeg_decode *)calloc(1, sizeof(*decode));
    if (!decode) return NULL;
    decode->codec.err = jpeg_std_error(&decode->error);
    decode->error.error_exit = fyto_jpeg_error;
    decode->error.output_message = fyto_jpeg_silent;
    decode->codec.client_data = decode;
    decode->stream = stream;
    decode->bytes = bytes;
    decode->size = size;
    decode->cancelled = cancelled;
    decode->context = context;
    if (setjmp(decode->jump)) goto done;
    jpeg_create_decompress(&decode->codec);
    decode->input.init_source = fyto_jpeg_source_start;
    decode->input.fill_input_buffer = fyto_jpeg_source_fill;
    decode->input.skip_input_data = fyto_jpeg_source_skip;
    decode->input.resync_to_restart = jpeg_resync_to_restart;
    decode->input.term_source = fyto_jpeg_source_end;
    decode->codec.src = &decode->input;
    if (jpeg_read_header(&decode->codec, TRUE) != JPEG_HEADER_OK ||
        decode->codec.data_precision != 8 ||
        decode->codec.image_width != expected_wide ||
        decode->codec.image_height != expected_high ||
        (decode->codec.jpeg_color_space != JCS_YCbCr &&
         decode->codec.jpeg_color_space != JCS_RGB &&
         decode->codec.jpeg_color_space != JCS_GRAYSCALE)) goto done;
    decode->codec.scale_num = 1;
    decode->codec.scale_denom = denominator;
    decode->codec.out_color_space = bit_count == 32 ? JCS_EXT_BGRX : JCS_EXT_BGR;
    if (!jpeg_start_decompress(&decode->codec)) goto done;
    *wide = decode->codec.output_width;
    *high = decode->codec.output_height;
    decode->bitmap = fyto_image_dib(*wide, *high, bit_count, &pixels);
    if (!decode->bitmap) goto done;
    stride = (UINT)((((UINT64)*wide * bit_count + 31) / 32) * 4);
    while (decode->codec.output_scanline < decode->codec.output_height)
    {
        JSAMPROW rows[16];
        JDIMENSION i, count = min(16, decode->codec.output_height - decode->codec.output_scanline);
        if (cancelled && cancelled(context)) goto done;
        for (i = 0; i < count; ++i)
            rows[i] = pixels + (size_t)(decode->codec.output_scanline + i) * stride;
        if (!jpeg_read_scanlines(&decode->codec, rows, count)) goto done;
    }
    if (!jpeg_finish_decompress(&decode->codec) ||
        (cancelled && cancelled(context))) goto done;
    result = decode->bitmap;
    decode->bitmap = NULL;
done:
    if (decode->bitmap) DeleteObject(decode->bitmap);
    jpeg_destroy_decompress(&decode->codec);
    free(decode);
    return result;
}
static HBITMAP fyto_jpeg_decode_pixels(const BYTE *bytes, size_t size,
    UINT expected_wide, UINT expected_high, UINT denominator, UINT bit_count,
    UINT *wide, UINT *high, BOOL (*cancelled)(void *), void *context)
{
    return fyto_jpeg_decode_input(NULL, bytes, size, expected_wide, expected_high,
        denominator, bit_count, wide, high, cancelled, context);
}
#endif

