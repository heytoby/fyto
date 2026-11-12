from pathlib import Path
from test_playback_contract import body
root=Path(__file__).resolve().parents[1]
source=(root/"voidImageViewer/src/viv.c").read_text(encoding="utf-8-sig")
generated=root/"build/presentation-under-test.h"
generated.parent.mkdir(parents=True,exist_ok=True)
generated.write_text("\n\n".join(body(source,name) for name in ["_viv_remember_manual_size","_viv_fit_image_window","_viv_mode_window_rect","_viv_update_frame","_viv_set_view_mode","_viv_start_first_frame"])+"\n",encoding="utf8")
(root/"build/render-under-test.h").write_text("\n\n".join(body(source,name) for name in ["_viv_render_size_in_area","_viv_get_render_size"])+"\n",encoding="utf8")
bitmap_source=(root/"voidImageViewer/src/fast_jpeg.h").read_text(encoding="utf-8-sig")
mipmap_functions=[body(bitmap_source,"fyto_image_dib")]
mipmap_functions += [body(source,name) for name in ("_viv_average_pixels","_viv_sum_pixel_pairs","_viv_half_dib")]
(root/"build/mipmap-under-test.h").write_text("\n\n".join(mipmap_functions)+"\n",encoding="utf8")
assert "_viv_pending_open_fullscreen" not in source
assert "_viv_toggle_fullscreen" not in body(source,"_viv_start_first_frame")
title=(root/"voidImageViewer/src/titlebar.h").read_text(encoding="utf-8-sig")
assert "fyto_caption_create_font(os_logical_high)" in title
assert "fyto_caption_filename_rect(&bar," in title
print("presentation tests use actual rendering size, fitting and first-frame implementations; captions share metrics")

