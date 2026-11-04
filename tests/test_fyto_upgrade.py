from window_test_helpers import *
root=Path('work/fyto-upgrade-check-v2').resolve();root.mkdir(exist_ok=True)
shutil.copy2('work/fyto-source/voidImageViewer/vs2019/x64/Release/fyto.exe',root/'fyto.exe')
(root/'fyto.ini').write_text('[fyto]\nappdata=0\nmultiple_instances=1\n')
(root/'voidImageViewer.ini').write_text('[voidImageViewer]\nappdata=0\nmultiple_instances=1\ndefaults_version=2\nwindowed_background_color_r=17\nwindowed_background_color_g=23\nwindowed_background_color_b=31\nleft_click_action=5\nmouse_wheel_action=1\nsort_ascending=1\n')
p=subprocess.Popen([str(root/'fyto.exe')],cwd=root)
wait(lambda:any(n=='fyto' for h,n in wins(p.pid)));h=next(h for h,n in wins(p.pid) if n=='fyto');time.sleep(1)
assert not any(n=='#32770' for _,n in wins(p.pid));u.PostMessageW(h,0x10,0,0);p.wait(timeout=5)
s=(root/'fyto.ini').read_text(encoding='utf-8-sig');assert 'windowed_background_color_r=17' in s and 'left_click_action=5' in s,s
fresh=Path('work/fyto-first-check-v2').resolve();fresh.mkdir(exist_ok=True);shutil.copy2(root/'fyto.exe',fresh/'fyto.exe');(fresh/'fyto.ini').write_text('[fyto]\nappdata=0\nmultiple_instances=1\n')
p=subprocess.Popen([str(fresh/'fyto.exe')],cwd=fresh)
try:
 wait(lambda:any(n=='#32770' for _,n in wins(p.pid)));dialog=next(h for h,n in wins(p.pid) if n=='#32770');u.PostMessageW(dialog,0x111,7,0);time.sleep(.2)
 h=next(h for h,n in wins(p.pid) if n=='fyto');u.PostMessageW(h,0x10,0,0);p.wait(timeout=5)
finally:
 if p.poll() is None:p.terminate()
assert (fresh/'first-run.complete').exists()
p=subprocess.Popen([str(fresh/'fyto.exe')],cwd=fresh);wait(lambda:any(n=='fyto' for _,n in wins(p.pid)));time.sleep(.9);assert not any(n=='#32770' for _,n in wins(p.pid));h=next(h for h,n in wins(p.pid) if n=='fyto');u.PostMessageW(h,0x10,0,0);p.wait(timeout=5)
print('Legacy settings retained; upgrade does not prompt; fresh install prompt can be declined and does not recur')
