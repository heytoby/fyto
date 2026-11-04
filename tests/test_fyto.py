from window_test_helpers import *
root=Path('work/fyto-check-v2').resolve();root.mkdir(exist_ok=True)
shutil.copy2('work/fyto-source/voidImageViewer/vs2019/x64/Release/fyto.exe',root/'fyto.exe')
(root/'fyto.ini').write_text('[fyto]\nappdata=0\nmultiple_instances=1\nlanguage=1\n')
Image.new('RGB',(1800,1200),(30,120,80)).save(root/'photo.png')
u.GetMenu.argtypes=[w.HWND];u.GetMenu.restype=w.HMENU
p=subprocess.Popen([str(root/'fyto.exe'),str(root/'photo.png')],cwd=root)
try:
 wait(lambda:any(n=='fyto' for h,n in wins(p.pid)));h=next(h for h,n in wins(p.pid) if n=='fyto')
 time.sleep(1);assert any(n=='ToolbarWindow32' for _,n in wins(p.pid,h));assert not any(n=='FYTO_SPLASH' for _,n in wins(p.pid));assert not u.GetMenu(h)
 dwm=c.WinDLL('dwmapi');dwm.DwmGetWindowAttribute.argtypes=[w.HWND,w.DWORD,c.c_void_p,w.DWORD];corner=c.c_int();hr=dwm.DwmGetWindowAttribute(h,33,c.byref(corner),4)
 assert hr!=0 or corner.value==1
 capture(h,root/'standard.png')
 for i in range(4):u.PostMessageW(h,0x100,49,0);time.sleep(.15)
 assert any(n=='ToolbarWindow32' for _,n in wins(p.pid,h))
 print('fyto startup, splash timeout, standard default, mode toggles, detached menu passed; corner attribute',hr,corner.value)
finally:
 u.PostMessageW(h,0x10,0,0);p.wait(timeout=5)
