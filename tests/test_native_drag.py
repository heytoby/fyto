from window_test_helpers import *
u.ShowWindow.argtypes=[w.HWND,c.c_int];u.IsZoomed.argtypes=[w.HWND];u.SetCursorPos.argtypes=[c.c_int,c.c_int]
u.mouse_event.argtypes=[w.DWORD,w.DWORD,w.DWORD,w.DWORD,c.c_size_t]
root=Path('work/startup-fullscreen-check').resolve()
(root/'voidImageViewer.ini').write_text('[voidImageViewer]\nappdata=0\nmultiple_instances=1\nwide=64\nhigh=64\nmaximized=1\n')
p=subprocess.Popen([str(root/'voidImageViewer.exe'),str(root/'land.png')],cwd=root)
try:
 wait(lambda:any(n=='VOIDIMAGEVIEWER' for h,n in wins(p.pid)))
 h=next(h for h,n in wins(p.pid) if n=='VOIDIMAGEVIEWER');time.sleep(.5)
 before=rect(h);u.SetForegroundWindow(h);x=(before[0]+before[2])//2;y=before[1]+15
 assert u.SetCursorPos(x,y),'no desktop cursor access'
 time.sleep(.1);u.mouse_event(2,0,0,0,0)
 for delta in range(20,201,20):u.SetCursorPos(x+delta,y+delta);time.sleep(.05)
 u.mouse_event(4,0,0,0,0);time.sleep(.5)
 after=rect(h);result={'before':before,'after':after,'maximized':bool(u.IsZoomed(h))}
 print(json.dumps(result));assert not u.IsZoomed(h) and min(after[2]-after[0],after[3]-after[1])>200
 Path('work/native-drag-results.json').write_text(json.dumps(result,indent=2))
finally:
 u.mouse_event(4,0,0,0,0)
 u.PostMessageW(h,0x100,27,0)
 try:p.wait(timeout=5)
 except subprocess.TimeoutExpired:p.terminate()
