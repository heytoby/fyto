from window_test_helpers import *
import hashlib
u.SetProcessDPIAware()
u.MonitorFromWindow.argtypes=[w.HWND,w.DWORD];u.MonitorFromWindow.restype=w.HANDLE
u.GetWindowLongW.argtypes=[w.HWND,c.c_int];u.GetWindowLongW.restype=w.LONG
u.IsZoomed.argtypes=[w.HWND];u.IsZoomed.restype=w.BOOL
u.ShowWindow.argtypes=[w.HWND,c.c_int]
u.IsWindowVisible.argtypes=[w.HWND];u.IsWindowVisible.restype=w.BOOL
class MI(c.Structure):_fields_=[('size',w.DWORD),('monitor',w.RECT),('work',w.RECT),('flags',w.DWORD)]
u.GetMonitorInfoW.argtypes=[w.HANDLE,c.POINTER(MI)]
MCB=c.WINFUNCTYPE(w.BOOL,w.HANDLE,w.HDC,c.POINTER(w.RECT),w.LPARAM)
monitors=[]
@MCB
def enum(h,dc,r,param):
 info=MI();info.size=c.sizeof(info);assert u.GetMonitorInfoW(h,c.byref(info));monitors.append({'handle':h,'monitor':tuple(info.monitor),'work':tuple(info.work)});return True
# RECT is not iterable in ctypes.
def coords(r):return r.left,r.top,r.right,r.bottom
@MCB
def enum(h,dc,r,param):
 info=MI();info.size=c.sizeof(info);assert u.GetMonitorInfoW(h,c.byref(info));monitors.append({'handle':h,'monitor':coords(info.monitor),'work':coords(info.work)});return True
u.EnumDisplayMonitors(None,None,enum,0);monitors.sort(key=lambda m:m['monitor'][0])
import uuid
project=Path(__file__).resolve().parents[1]
root=project/('build/runtime-'+uuid.uuid4().hex[:8]);root.mkdir(parents=True)
shutil.copy2(project/'voidImageViewer/vs2019/x64/Release/fyto.exe',root/'fyto.exe')
(root/'fyto.ini').write_text('[fyto]\nappdata=0\nlanguage=1\nmultiple_instances=1\ndefaults_version=2\nauto_zoom=0\npreload_next=1\ncache_last=1\nshow_caption=1\nshow_menu=1\nshow_status=1\nshow_controls=1\nshow_thickframe=1\nmaximized=0\n')
(root/'associations.ini').write_text('[fyto]\nSelectedFormats=15\nSetupVersion=1\n')
photos=[('1-landscape.png',(3000,1500),(50,110,180)),('2-portrait.png',(1500,3000),(190,70,90)),('3-panorama.png',(4800,1200),(60,170,80)),('4-square.png',(2200,2200),(160,90,180))]
for name,size,color in photos:Image.new('RGB',size,color).save(root/name)
hashes={name:hashlib.sha256((root/name).read_bytes()).hexdigest() for name,_,_ in photos}
processes=[];report={'version':'1.0.0.28','geometry_matrix':2560,'monitors':monitors,'monitor_checks':[]}
def launch(window=False):
 p=subprocess.Popen([str(root/'fyto.exe')]+(['/window'] if window else [])+[str(root/photos[0][0])],cwd=root);processes.append(p);wait(lambda:any(n=='fyto' for _,n in wins(p.pid)));h=next(h for h,n in wins(p.pid) if n=='fyto');wait(lambda:label(h)==photos[0][0]);time.sleep(.5);return p,h
def close(p,h):u.PostMessageW(h,0x10,0,0);p.wait(timeout=5);assert p.returncode==0

def current_monitor(h):
 mh=u.MonitorFromWindow(h,2);return next(m for m in monitors if m['handle']==mh)
def is_full(h):return (u.GetWindowLongW(h,-16)&0x00c00000)==0

def client_size(h):
 r=w.RECT();u.GetClientRect(h,c.byref(r));return r.right,r.bottom

def photo_ready(h,index):
 if label(h)!=photos[index][0]:return False
 cw,ch=client_size(h);dc=u.GetDC(h);value=g.GetPixel(dc,cw//2,max(0,(ch-32)//2));u.ReleaseDC(h,dc)
 rgb=(value&255,(value>>8)&255,(value>>16)&255)
 return max(abs(a-b) for a,b in zip(rgb,photos[index][2]))<25

def fitted(h,index,name):
 wait(lambda:photo_ready(h,index));time.sleep(.2)
 image=capture(h,root/(name+'.png'));cw,ch=image.size;image_height=ch-(32 if any(n=='ToolbarWindow32' and u.IsWindowVisible(t) for t,n in wins(p.pid,h)) else 0)
 iw,ih=photos[index][1];error=abs(cw*ih-image_height*iw);assert error<=max(iw,ih),(name,'ratio',image.size,image_height,photos[index][1],rect(h))
 color=image.getpixel((cw//2,image_height//2))
 for x,y in [(2,2),(cw-3,2),(2,image_height-3),(cw-3,image_height-3)]:
  assert max(abs(a-b) for a,b in zip(image.getpixel((x,y)),color))<5,(name,'padding',x,y,image.getpixel((x,y)),color)
 return {'image':photos[index][0],'window':rect(h),'image_area':[cw,image_height]}

def next_photo(h,index,back=False):
 u.PostMessageW(h,0x111,1091 if back else 1092,0)
 try:wait(lambda:photo_ready(h,index))
 except Exception:
  image=capture(h,root/'navigation-failure.png');print('navigation failure',index,label(h),rect(h),image.size,image.getpixel((image.width//2,image.height//2)),flush=True);raise
 time.sleep(.2)

try:
 p,h=launch();m=current_monitor(h);assert rect(h)==m['monitor'] and is_full(h),(rect(h),m);report['startup']='fullscreen by default; no modal format prompt'
 u.PostMessageW(h,0x111,1024,0);wait(lambda:not is_full(h));time.sleep(.25);fitted(h,0,'first-window')
 for mon_index,m in enumerate(monitors):
  # Move only; a change of monitor must discard the old monitor's envelope.
  u.SetWindowPos(h,0,m['work'][0]+4,m['work'][1]+4,0,0,0x15);wait(lambda:current_monitor(h)['handle']==m['handle']);time.sleep(.35)
  checks=[fitted(h,0,f'm{mon_index}-wide')];start=rect(h)
  for index in range(1,4):next_photo(h,index);checks.append(fitted(h,index,f'm{mon_index}-{index}'))
  for index in range(2,-1,-1):next_photo(h,index,True);checks.append(fitted(h,index,f'm{mon_index}-back-{index}'))
  end=rect(h);assert abs((end[2]-end[0])-(start[2]-start[0]))<=1 and abs((end[3]-end[1])-(start[3]-start[1]))<=1,('cumulative shrink',start,end)
  work=m['work'];assert end[0]>=work[0] and end[1]>=work[1] and end[2]<=work[2] and end[3]<=work[3],(end,work)
  u.PostMessageW(h,0x111,1024,0);time.sleep(.3);assert is_full(h) and rect(h)==m['monitor'];u.PostMessageW(h,0x111,1024,0);time.sleep(.3);fitted(h,0,f'm{mon_index}-restored')
  report['monitor_checks'].append({'monitor':m['monitor'],'work':m['work'],'photos':checks,'fullscreen_on_this_monitor':True,'no_cumulative_shrink':True})
 # Fixed half-screen and two-thirds placements are not fitted when photos change.
 m=monitors[0];l,t,r,b=m['work']
 for numerator,denominator in [(1,2),(2,3)]:
  u.PostMessageW(h,0x231,0,0);u.SetWindowPos(h,0,l,t,(r-l)*numerator//denominator,b-t,0x14);u.PostMessageW(h,0x232,0,0);time.sleep(.3);fixed=rect(h)
  next_photo(h,1);assert rect(h)==fixed;next_photo(h,0,True);assert rect(h)==fixed
  u.PostMessageW(h,0x111,1016,0);time.sleep(.2);assert rect(h)==fixed;u.PostMessageW(h,0x111,1018,0);time.sleep(.2);assert rect(h)==fixed
 report['fixed_windows']='half and two-thirds placements retain their rectangle through navigation and mode changes'
 # An ordinary floating window follows image geometry in minimal mode too.
 u.PostMessageW(h,0x231,0,0);u.SetWindowPos(h,0,l+120,t+120,900,600,0x14);u.PostMessageW(h,0x232,0,0);time.sleep(.3)
 before=rect(h);u.PostMessageW(h,0x111,1016,0);time.sleep(.25);assert rect(h)==before
 next_photo(h,1);fitted(h,1,'minimal-portrait');next_photo(h,0,True);fitted(h,0,'minimal-landscape')
 report['minimal']='same image fitting applies in minimal mode; chrome-only toggle retains its outer rectangle'
 close(p,h)
 assert all(hashlib.sha256((root/name).read_bytes()).hexdigest()==hashes[name] for name,_,_ in photos)
 report['source_images']='unchanged'
finally:
 for proc in processes:
  if proc.poll() is None:
   for window,cls in wins(proc.pid):
    if cls=='fyto':u.PostMessageW(window,0x10,0,0)
   try:proc.wait(timeout=4)
   except subprocess.TimeoutExpired:proc.terminate();proc.wait(timeout=4)
(root/'validation.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))
