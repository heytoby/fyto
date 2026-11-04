from window_test_helpers import *
from PIL import ImageChops,ImageStat
import re,hashlib
shell=c.WinDLL('shell32');shell.ExtractIconExW.argtypes=[w.LPCWSTR,c.c_int,c.POINTER(w.HICON),c.POINTER(w.HICON),w.UINT];shell.ExtractIconExW.restype=w.UINT
u.DrawIconEx.argtypes=[w.HDC,c.c_int,c.c_int,w.HICON,c.c_int,c.c_int,w.UINT,w.HBRUSH,w.UINT];u.DestroyIcon.argtypes=[w.HICON]
u.FillRect.argtypes=[w.HDC,c.POINTER(w.RECT),w.HBRUSH];g.CreateSolidBrush.argtypes=[w.DWORD];g.CreateSolidBrush.restype=w.HBRUSH
u.GetWindowDC.argtypes=[w.HWND];u.GetWindowDC.restype=w.HDC
u.GetDlgItem.argtypes=[w.HWND,c.c_int];u.GetDlgItem.restype=w.HWND
u.GetWindowTextLengthW.argtypes=[w.HWND];u.GetWindowTextLengthW.restype=c.c_int
root=Path('work/fyto-v22-check').resolve();root.mkdir(exist_ok=True)
exe=Path('work/fyto-source/voidImageViewer/vs2019/x64/Release/fyto.exe').resolve();shutil.copy2(exe,root/'fyto.exe')
(root/'fyto.ini').write_text('[fyto]\nappdata=0\nmultiple_instances=1\nlanguage=1\ndefaults_version=2\n');(root/'first-run.complete').write_text('test')
report={'version':'1.0.0.22','icon_checks':[]}
def from_bitmap(dc,bm,width,height):
 bi=BI();bi.size=c.sizeof(BI);bi.width=width;bi.height=-height;bi.planes=1;bi.bits=32;data=c.create_string_buffer(width*height*4)
 assert g.GetDIBits(dc,bm,0,height,data,c.byref(bi),0)
 return Image.frombytes('RGB',(width,height),data.raw,'raw','BGRX')
large=w.HICON();small=w.HICON();assert shell.ExtractIconExW(str(exe),0,c.byref(large),c.byref(small),1)
for size,icon in [(32,large),(16,small)]:
 for bg in [(48,64,80),(224,216,208)]:
  dc=u.GetDC(None);mem=g.CreateCompatibleDC(dc);bm=g.CreateCompatibleBitmap(dc,size,size);old=g.SelectObject(mem,bm)
  brush=g.CreateSolidBrush(bg[0]|bg[1]<<8|bg[2]<<16);r=w.RECT(0,0,size,size);u.FillRect(mem,c.byref(r),brush);g.DeleteObject(brush)
  assert u.DrawIconEx(mem,0,0,icon,size,size,0,None,3)
  g.SelectObject(mem,old);im=from_bitmap(mem,bm,size,size);bounds=ImageChops.difference(im,Image.new('RGB',im.size,bg)).getbbox();assert bounds
  width=bounds[2]-bounds[0];height=bounds[3]-bounds[1]
  assert width==round(size*.75) and height==round(width*435/740),(size,bounds)
  assert im.getpixel((0,0))==bg and im.getpixel((size-1,size-1))==bg
  report['icon_checks'].append({'slot':size,'background':bg,'visible_bounds':bounds})
  im.save(root/f'icon-{size}-{bg[0]}.png');g.DeleteObject(bm);g.DeleteDC(mem);u.ReleaseDC(None,dc)
 u.DestroyIcon(icon)
def window_image(h):
 r=rect(h);width=r[2]-r[0];height=r[3]-r[1];dc=u.GetWindowDC(h);mem=g.CreateCompatibleDC(dc);bm=g.CreateCompatibleBitmap(dc,width,height);old=g.SelectObject(mem,bm);assert u.PrintWindow(h,mem,0)
 g.SelectObject(mem,old);im=from_bitmap(mem,bm,width,height);g.DeleteObject(bm);g.DeleteDC(mem);u.ReleaseDC(h,dc);return im
# Resolve the actual command ID from the project's enum.
s=Path('work/fyto-source/voidImageViewer/src/viv.h').read_text(encoding='utf-8-sig');enum=s[s.index('VIV_ID_EDIT_CUT = 1000'):];enum=enum[:enum.index('};')];enum=re.sub(r'//[^\n]*','',enum);ids={};value=999
for part in enum.split(','):
 m=re.fullmatch(r'\s*(VIV_ID_[A-Z0-9_]+)(?:\s*=\s*(0x[0-9a-fA-F]+|\d+))?\s*',part)
 if not m:continue
 value=int(m[2],0) if m[2] else value+1;ids[m[1]]=value
p=subprocess.Popen([str(root/'fyto.exe')],cwd=root)
try:
 wait(lambda:any(n=='fyto' for _,n in wins(p.pid)));h=next(h for h,n in wins(p.pid) if n=='fyto');time.sleep(.6)
 # No added logo or reserved gap before the first menu label.
 for width in [1100,600]:
  u.SetWindowPos(h,None,100,100,width,650,0x14);time.sleep(.2)
  im=window_image(h);corner=im.getpixel((1,1));strip=im.crop((10,10,20,35));assert ImageChops.difference(strip,Image.new('RGB',strip.size,corner)).getbbox() is None
  hit=u.SendMessageW(h,0x84,0,(100+40)|((100+20)<<16));assert hit==5,('menu target',width,hit)
 report['menu_logo']='absent in wide and compact menu layouts; first menu hit target passed'
 assert not any(n=='FYTO_SPLASH' for _,n in wins(p.pid))
 for cycle in range(2):
  u.PostMessageW(h,0x111,ids['VIV_ID_HELP_ABOUT'],0);wait(lambda:any(n=='#32770' for _,n in wins(p.pid)))
  about=next(t for t,n in wins(p.pid) if n=='#32770');time.sleep(.3);picture=u.GetDlgItem(about,60010);assert picture
  pr=rect(picture);origin=w.POINT(0,0);u.ClientToScreen(about,c.byref(origin));wimg=pr[2]-pr[0];himg=pr[3]-pr[1]
  assert abs(himg-wimg*1535/2048)<=1,(wimg,himg)
  screenshot=capture(about,root/f'about-{cycle}.png');actual=screenshot.crop((pr[0]-origin.x,pr[1]-origin.y,pr[2]-origin.x,pr[3]-origin.y))
  original=Image.open('work/fyto-assets/splash.png').convert('RGBA');expected=Image.new('RGBA',original.size,'white');expected.alpha_composite(original);expected=expected.convert('RGB').resize((wimg,himg),Image.Resampling.BICUBIC)
  error=ImageStat.Stat(ImageChops.difference(actual,expected)).mean;assert max(error)<4,(cycle,error)
  assert '1.0.0.22' in label(u.GetDlgItem(about,1039))
  assert 'void Image Viewer' in label(u.GetDlgItem(about,1037)) and 'Copyright 2025' in label(u.GetDlgItem(about,1040))
  report['about']={'image_display':[wimg,himg],'source_image':[2048,1535],'mean_pixel_difference':error,'source_credits':'retained','reopen_cycles':cycle+1}
  if cycle==0:
   u.PostMessageW(about,0x111,1041,0);wait(lambda:len([t for t,n in wins(p.pid) if n=='#32770'])==2)
   license_window=next(t for t,n in wins(p.pid) if n=='#32770' and t!=about);control=u.GetDlgItem(license_window,60012);wait(lambda:u.SendMessageW(control,0x0e,0,0)>1000);length=u.SendMessageW(control,0x0e,0,0);text=c.create_unicode_buffer(length+1);u.SendMessageW(control,0x0d,length+1,c.cast(text,c.c_void_p).value)
   assert 'Copyright 2025 voidtools / David Carpenter' in text.value and 'Google Inc.' in text.value and 'Permission is hereby granted' in text.value
   u.PostMessageW(license_window,0x111,1,0);time.sleep(.1);report['licenses']='full MIT and WebP notices open inside the app'
  u.PostMessageW(about,0x111,1,0);wait(lambda:not any(n=='#32770' for _,n in wins(p.pid)))
finally:
 u.PostMessageW(h,0x10,0,0)
 try:p.wait(timeout=5)
 except subprocess.TimeoutExpired:p.terminate()
Path('work/fyto-v22-ui-validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(report))
