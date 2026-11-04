from window_test_helpers import *
import sys
u.ShowWindow.argtypes=[w.HWND,c.c_int];u.IsZoomed.argtypes=[w.HWND]
u.GetWindowLongW.argtypes=[w.HWND,c.c_int];u.GetWindowLongW.restype=w.LONG
class WP(c.Structure):
 _fields_=[('length',w.UINT),('flags',w.UINT),('showCmd',w.UINT),('min',w.POINT),('max',w.POINT),('normal',w.RECT)]
u.GetWindowPlacement.argtypes=[w.HWND,c.POINTER(WP)]
root=Path('work/startup-fullscreen-check').resolve();root.mkdir(exist_ok=True)
shutil.copy2(sys.argv[1],root/'voidImageViewer.exe')
for n,sz in [('land.png',(1800,1200)),('portrait.png',(1200,1800))]:Image.new('RGB',sz,(30,120,80)).save(root/n)
def launch(name):
 p=subprocess.Popen([str(root/'voidImageViewer.exe'),str(root/name)],cwd=root)
 wait(lambda:any(n=='VOIDIMAGEVIEWER' for h,n in wins(p.pid)))
 h=next(h for h,n in wins(p.pid) if n=='VOIDIMAGEVIEWER');wait(lambda:label(h)==name);time.sleep(.4)
 return p,h
def close(p,h):u.PostMessageW(h,0x100,27,0);p.wait(timeout=5)
def size(h):r=rect(h);return [r[2]-r[0],r[3]-r[1]]
results=[]
for stale in [(64,64),(300,600),(800,200)]:
 (root/'voidImageViewer.ini').write_text(f'[voidImageViewer]\nappdata=0\nmultiple_instances=1\nwide={stale[0]}\nhigh={stale[1]}\n',encoding='ascii')
 seq=[]
 for n in ['land.png','portrait.png']*3:
  p,h=launch(n)
  try:seq.append(size(h));u.PostMessageW(h,0x100,49,0);time.sleep(.1)
  finally:close(p,h)
 results.append({'stale':stale,'sizes':seq})
 if '--assert' in sys.argv:assert seq==results[0]['sizes'] and seq[0]==seq[2]==seq[4] and seq[1]==seq[3]==seq[5],results
(root/'voidImageViewer.ini').write_text('[voidImageViewer]\nappdata=0\nmultiple_instances=1\nwide=64\nhigh=64\nmaximized=1\n')
p,h=launch('land.png')
try:
 wp=WP();wp.length=c.sizeof(wp);u.GetWindowPlacement(h,c.byref(wp));r=wp.normal;restore=[r.right-r.left,r.bottom-r.top]
 u.ShowWindow(h,9);time.sleep(.2);results.append({'restore':restore,'restored':size(h)})
 if '--assert' in sys.argv:assert min(restore)>200 and min(size(h))>200
 # Fullscreen command, then key 1 twice: controls and title must appear/disappear without resizing.
 u.SendMessageW(h,0x111,1024,0);time.sleep(.3);full=rect(h)
 states=[]
 for key in [49,49,49,49]:
  u.PostMessageW(h,0x100,key,0);time.sleep(.3)
  toolbar=any(cls=='ToolbarWindow32' for _,cls in wins(p.pid,h));client=w.RECT();u.GetClientRect(h,c.byref(client));states.append([toolbar,client.bottom]);
  if '--assert' in sys.argv:assert rect(h)==full
 results.append({'fullscreen_toggle':states})
 if '--assert' in sys.argv:assert [x[0] for x in states]==[True,False,True,False] and states[0][1]<states[1][1]
 u.SendMessageW(h,0x111,1024,0);time.sleep(.2)
 if '--assert' in sys.argv:assert u.GetWindowLongW(h,-16)&0x00c40000==0x00c40000
finally:close(p,h)
print(json.dumps(results),flush=True)
Path('work/startup-fullscreen-results.json').write_text(json.dumps(results,indent=2))
