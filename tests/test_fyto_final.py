from window_test_helpers import *
root=Path('work/fyto-final-check').resolve();root.mkdir(exist_ok=True);shutil.copy2('work/fyto-source/voidImageViewer/vs2019/x64/Release/fyto.exe',root/'fyto.exe');(root/'fyto.ini').write_text('[fyto]\nappdata=0\nmultiple_instances=1\n');(root/'first-run.complete').write_text('test')
p=subprocess.Popen([str(root/'fyto.exe')],cwd=root)
try:
 wait(lambda:any(n=='fyto' for h,n in wins(p.pid)));h=next(h for h,n in wins(p.pid) if n=='fyto');time.sleep(.8)
 assert any(n=='ToolbarWindow32' for _,n in wins(p.pid,h))
 u.PostMessageW(h,0x111,60003,0);wait(lambda:any(n=='#32770' for _,n in wins(p.pid)));d=next(t for t,n in wins(p.pid) if n=='#32770')
 assert sum(1 for _,n in wins(p.pid,d) if n=='Button')==12,wins(p.pid,d)
 u.PostMessageW(d,0x111,2,0);time.sleep(.2)
 capture(h,root/'standard.png');print('standard mode, format selection dialog with ten formats, cancel, startup rendering passed')
finally:u.PostMessageW(h,0x10,0,0);p.wait(timeout=5)
