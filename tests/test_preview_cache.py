from pathlib import Path
import sys
from test_playback_contract import body
r=Path(__file__).resolve().parents[1];s=(r/'voidImageViewer/src/viv.c').read_text(encoding='utf8')
p=r/'build/cache-under-test.h';p.parent.mkdir(exist_ok=True)
p.write_text(body(s,'_viv_take_recent_preview')+'\n'+body(s,'_viv_remember_preview')+'\n'+body(s,'_viv_defer_detail_upgrade'),encoding='utf8')
