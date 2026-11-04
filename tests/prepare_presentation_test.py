from pathlib import Path
from test_playback_contract import body
root=Path(__file__).resolve().parents[1]
source=(root/"voidImageViewer/src/viv.c").read_text(encoding="utf-8-sig")
generated=root/"build/presentation-under-test.h"
generated.parent.mkdir(parents=True,exist_ok=True)
generated.write_text(body(source,"_viv_fit_image_window")+"\n\n"+body(source,"_viv_start_first_frame")+"\n",encoding="utf8")
assert "_viv_pending_open_fullscreen" not in source
assert "_viv_toggle_fullscreen" not in body(source,"_viv_start_first_frame")
print("presentation tests use the actual fit and first-frame implementations")

