import re,hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
NAMES=["_viv_pause","_viv_slideshow","_viv_set_rate","_viv_set_custom_rate","_viv_increase_rate","_viv_animation_pause","_viv_timer_start","_viv_timer_stop","_viv_next"]
def body(source,name):
    match=re.search(r"^static [^;\n{}]*\b"+re.escape(name)+r"\([^;{}]*\)\s*\{",source,re.M)
    if not match:raise AssertionError("missing playback function "+name)
    start=source.index("{",match.start());i=start;depth=0;state="code"
    while i<len(source):
        ch=source[i];following=source[i:i+2]
        if state=="line":
            if ch=="\n":state="code"
        elif state=="block":
            if following=="*/":state="code";i+=1
        elif state in ["string","char"]:
            if ch=="\\":i+=1
            elif ch==('"' if state=="string" else "'"):state="code"
        else:
            if following=="//":state="line";i+=1
            elif following=="/*":state="block";i+=1
            elif ch=='"':state="string"
            elif ch=="'":state="char"
            elif ch=="{":depth+=1
            elif ch=="}":
                depth-=1
                if depth==0:return source[match.start():i+1]
        i+=1
    raise AssertionError("unterminated playback function "+name)
def hashes(source):
    result={name:hashlib.sha256(body(source,name).encode()).hexdigest() for name in NAMES}
    start=source.index("case VIV_ID_SLIDESHOW_TIMER:")
    end=source.index("case VIV_ID_HIDE_CURSOR_TIMER:",start)
    result["slideshow_timer"]=hashlib.sha256(source[start:end].encode()).hexdigest()
    return result
if __name__=="__main__":
    source=(ROOT/"voidImageViewer/src/viv.c").read_text(encoding="utf-8-sig")
    actual=hashes(source)
    expected=json.loads((ROOT/"tests/playback-contract.json").read_text())
    assert actual==expected,"Playback implementation changed"
    print("playback contract passed: 9 original functions and slideshow timer are unchanged")

