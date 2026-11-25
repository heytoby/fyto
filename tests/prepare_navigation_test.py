from pathlib import Path
from test_playback_contract import body
root=Path(__file__).resolve().parents[1]
s=(root/'voidImageViewer/src/viv.c').read_text(encoding='utf-8')
wheel=body(s,'_viv_do_mousewheel_action')
start=wheel.index('else if (action == 1 || action == 2)')
wheel='static void wheel(int action,int delta){'+wheel[start+5:]
out=root/'build/navigation-under-test.h'
out.parent.mkdir(exist_ok=True)
out.write_text('\n'.join(body(s,n) for n in ['_viv_replay_directory_navigation','_viv_next_manual'])+'\n'+wheel,encoding='utf-8')
