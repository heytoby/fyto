from window_test_helpers import *
u.GetWindowDC.argtypes=[w.HWND];u.GetWindowDC.restype=w.HDC
u.ShowWindow.argtypes=[w.HWND,c.c_int]
def window_pixels(h):
 r=rect(h);width=r[2]-r[0];height=r[3]-r[1];dc=u.GetWindowDC(h);mem=g.CreateCompatibleDC(dc);bm=g.CreateCompatibleBitmap(dc,width,height);old=g.SelectObject(mem,bm);assert u.PrintWindow(h,mem,0)
 area=w.RECT();u.SystemParametersInfoW(48,0,c.byref(area),0)
 left=max(0,area.left-r[0])+1;top=max(0,area.top-r[1])+1;right=min(width,area.right-r[0])-2;bottom=min(height,area.bottom-r[1])-2
 pts=[(left,top),(width//2,top),(right,top),(left,height//2),(right,height//2),(left,bottom),(width//2,bottom)]
 values=[g.GetPixel(mem,x,y) for x,y in pts]
 g.SelectObject(mem,old);g.DeleteObject(bm);g.DeleteDC(mem);u.ReleaseDC(h,dc);return values
u.GetWindowRgn.argtypes=[w.HWND,w.HRGN];g.CreateRectRgn.argtypes=[c.c_int,c.c_int,c.c_int,c.c_int];g.CreateRectRgn.restype=w.HRGN;g.PtInRegion.argtypes=[w.HRGN,c.c_int,c.c_int]
root=Path('work/fyto-opaque-check-v3').resolve();root.mkdir(exist_ok=True);shutil.copy2('work/fyto-source/voidImageViewer/vs2019/x64/Release/fyto.exe',root/'fyto.exe');(root/'first-run.complete').write_text('test');results=[]
for rgb in [(0,0,0),(23,35,47)]:
 (root/'fyto.ini').write_text(f'[fyto]\nappdata=0\nmultiple_instances=1\ndefaults_version=2\nwindowed_background_color_r={rgb[0]}\nwindowed_background_color_g={rgb[1]}\nwindowed_background_color_b={rgb[2]}\n')
 p=subprocess.Popen([str(root/'fyto.exe')],cwd=root)
 try:
  wait(lambda:any(n=='fyto' for _,n in wins(p.pid)));h=next(h for h,n in wins(p.pid) if n=='fyto');time.sleep(.8)
  bg=rgb[0]|rgb[1]<<8|rgb[2]<<16
  for mode in ['standard','minimal','standard','maximized']:
   if mode=='maximized':u.ShowWindow(h,3)
   elif len(results)>0 and results[-1].get('rgb')==list(rgb):u.PostMessageW(h,0x100,49,0)
   time.sleep(.2)
   r=rect(h);region=g.CreateRectRgn(0,0,0,0);assert u.GetWindowRgn(h,region)>0
   assert all(g.PtInRegion(region,x,y) for x,y in [(0,0),(r[2]-r[0]-1,0),(0,r[3]-r[1]-1),(r[2]-r[0]-1,r[3]-r[1]-1)])
   g.DeleteObject(region)
   assert u.SendMessageW(h,0x7f,1,0) and u.SendMessageW(h,0x7f,0,0)
   assert not any(n=='FYTO_SPLASH' for _,n in wins(p.pid))
   values=window_pixels(h);assert all(v==bg for v in values),(rgb,mode,values,bg);results.append({'rgb':list(rgb),'mode':mode,'edge_pixels':values})
 finally:
  u.PostMessageW(h,0x10,0,0)
  try:p.wait(timeout=5)
  except subprocess.TimeoutExpired:p.terminate()
Path('work/fyto-opaque-results.json').write_text(json.dumps(results,indent=2));print('Opaque menu/background and seven edge samples passed in black/custom colors, standard/minimal/maximized modes')
