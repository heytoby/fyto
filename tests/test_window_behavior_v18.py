from window_test_helpers import *
u.GetWindowLongW.argtypes=[w.HWND,c.c_int];u.GetWindowLongW.restype=w.LONG
u.ShowWindow.argtypes=[w.HWND,c.c_int];u.IsZoomed.argtypes=[w.HWND]
root=Path('work/window-behavior-check').resolve();root.mkdir(exist_ok=True)
shutil.copy2('work/startup-repair-source/voidImageViewer/vs2019/x64/Release/voidImageViewer.exe',root/'voidImageViewer.exe')
for name,size in [('1.png',(1200,600)),('2.png',(600,1200)),('3.png',(900,900))]:Image.new('RGB',size,(30,120,80)).save(root/name)
area=w.RECT();u.SystemParametersInfoW(48,0,c.byref(area),0)
results=[]
for preload in [0,1]:
 (root/'voidImageViewer.ini').write_text(f'[voidImageViewer]\nappdata=0\nmultiple_instances=1\nwide=800\nhigh=600\nauto_fit_wide_mul=800\nauto_fit_wide_div={area.right-area.left}\nauto_fit_high_mul=600\nauto_fit_high_div={area.bottom-area.top}\npreload_next={preload}\nwindowed_background_color_r=255\nwindowed_background_color_g=255\nwindowed_background_color_b=255\nleft_click_action=6\n',encoding='ascii')
 p=subprocess.Popen([str(root/'voidImageViewer.exe'),str(root/'1.png')],cwd=root)
 try:
  wait(lambda:any(n=='VOIDIMAGEVIEWER' for h,n in wins(p.pid)));h=next(h for h,n in wins(p.pid) if n=='VOIDIMAGEVIEWER');time.sleep(1)
  for delta,name,sz in [(0,'1.png',(800,400)),(-120,'2.png',(300,600)),(-120,'3.png',(600,600)),(120,'2.png',(300,600)),(120,'1.png',(800,400))]:
   if delta:u.SendMessageW(h,0x20a,(delta&65535)<<16,0)
   wait(lambda:label(h)==name);time.sleep(.4)
   im=capture(h,root/f'{preload}-{name}');assert im.size==sz,(preload,name,im.size,sz)
   assert im.getpixel((im.width//2,1))==(30,120,80),('top border',name)
   assert im.getpixel((im.width//2,im.height-2))==(30,120,80),('bottom border',name)
  results.append(f'preload {preload}: landscape/portrait/square resize and reverse navigation passed')
  r=rect(h);pt=(r[0]+100)|((r[1]+15)<<16);assert u.SendMessageW(h,0x84,0,pt)==2
  pt=(r[0]+100)|((r[1]+100)<<16);assert u.SendMessageW(h,0x84,0,pt)==1
  assert u.GetWindowLongW(h,-16)&0x00c40000==0x00c40000
  before=rect(h);u.PostMessageW(h,0x201,1,100|(100<<16));time.sleep(.1);u.PostMessageW(h,0x200,1,150|(120<<16));u.PostMessageW(h,0x202,0,150|(120<<16));time.sleep(.2);assert rect(h)==before
  results.append('native caption drag hit target and pan area passed; left drag retains window position')
  u.ShowWindow(h,3);time.sleep(.4);before=rect(h);u.SendMessageW(h,0x20a,(-120&65535)<<16,0);time.sleep(.5);assert rect(h)==before and u.IsZoomed(h)
  # Toggle standard mode while maximized; controls center on full client.
  u.PostMessageW(h,0x100,49,0);time.sleep(.6)
  tool=next(t for t,cls in wins(p.pid,h) if cls=='ToolbarWindow32');tr=rect(tool);wr=rect(h)
  assert abs((tr[0]+tr[2])-(wr[0]+wr[2]))<=2,(tr,wr)
  capture(h,root/f'{preload}-standard.png');results.append('maximized navigation retained geometry; whole-window toolbar center passed')
  u.PostMessageW(h,0x100,27,0);p.wait(timeout=5)
  ini=(root/'voidImageViewer.ini').read_text(encoding='utf-8-sig');assert 'left_click_action=0' in ini and 'windowed_background_color_r=0' in ini and 'defaults_version=1' in ini
 finally:
  if p.poll() is None:p.terminate()
Path('work/window-behavior-results.json').write_text(json.dumps(results,indent=2));print(json.dumps(results))
