from window_test_helpers import *
from PIL import ImageChops
u.GetMenu.argtypes=[w.HWND];u.GetMenu.restype=w.HMENU
root=Path('work/menu-hover-check').resolve();root.mkdir(exist_ok=True)
shutil.copy2('work/startup-repair-source/voidImageViewer/vs2019/x64/Release/voidImageViewer.exe',root/'voidImageViewer.exe')
Image.new('RGB',(1800,1200),(30,120,80)).save(root/'photo.png')
(root/'voidImageViewer.ini').write_text('[voidImageViewer]\nappdata=0\nmultiple_instances=1\nlanguage=1\n')
p=subprocess.Popen([str(root/'voidImageViewer.exe'),str(root/'photo.png')],cwd=root)
results=[]
try:
 wait(lambda:any(n=='VOIDIMAGEVIEWER' for h,n in wins(p.pid)))
 h=next(h for h,n in wins(p.pid) if n=='VOIDIMAGEVIEWER');time.sleep(.5)
 for fullscreen in [False,True]:
  if fullscreen:u.SendMessageW(h,0x111,1024,0);time.sleep(.2)
  for cycle in range(3):
   u.PostMessageW(h,0x100,49,0);time.sleep(.2);assert not u.GetMenu(h)
   before=capture(h,root/'before.png');r=rect(h)
   for x in [25,70,120,175,230,285,340]:
    u.SendMessageW(h,0xa0,5,(r[0]+x)|((r[1]+20)<<16))
   u.SendMessageW(h,0x2a2,0,0);time.sleep(.1)
   after=capture(h,root/'after.png');assert ImageChops.difference(before,after).getbbox() is None
   # Mouse click must still open the custom dropdown.
   u.PostMessageW(h,0xa1,5,(r[0]+25)|((r[1]+20)<<16));time.sleep(.2)
   assert any(cls=='#32768' for _,cls in wins(p.pid)),wins(p.pid)
   u.PostMessageW(h,0x1f,0,0);time.sleep(.2)
   u.PostMessageW(h,0x100,49,0);time.sleep(.2);assert not u.GetMenu(h)
  results.append({'fullscreen':fullscreen,'toggles':6,'hover':'no client artifacts','dropdown':'opened/cancelled'})
finally:
 u.PostMessageW(h,0x10,0,0)
 try:p.wait(timeout=5)
 except subprocess.TimeoutExpired:p.terminate()
Path('work/menu-hover-results.json').write_text(json.dumps(results,indent=2));print(json.dumps(results))
