#ifndef FYTO_TEXT_RENDER_H
#define FYTO_TEXT_RENDER_H

#ifdef __cplusplus
extern "C" {
#endif

BOOL fyto_text_initialize(void);
void fyto_text_shutdown(void);
BOOL fyto_text_render(HDC dc,const wchar_t *text,int length,RECT *rect,UINT flags,int pixels);

#ifdef __cplusplus
}
#endif

#endif
