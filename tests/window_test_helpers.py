from pathlib import Path
import ctypes as c,ctypes.wintypes as w,subprocess,time,json,shutil
from PIL import Image,ImageGrab
u=c.WinDLL('user32');u.GetWindowThreadProcessId.argtypes=[w.HWND,c.POINTER(w.DWORD)];u.GetClassNameW.argtypes=[w.HWND,w.LPWSTR,c.c_int];u.GetWindowTextW.argtypes=[w.HWND,w.LPWSTR,c.c_int];u.SendMessageW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM];u.GetClientRect.argtypes=[w.HWND,c.POINTER(w.RECT)];u.ClientToScreen.argtypes=[w.HWND,c.POINTER(w.POINT)];u.SetForegroundWindow.argtypes=[w.HWND]
CB=c.WINFUNCTYPE(w.BOOL,w.HWND,w.LPARAM)
def wins(pid,parent=None):
 out=[]
 @CB
 def cb(h,l):
  p=w.DWORD();u.GetWindowThreadProcessId(h,c.byref(p))
  if p.value==pid:
   b=c.create_unicode_buffer(256);u.GetClassNameW(h,b,256);out.append((h,b.value))
  return True
 if parent:u.EnumChildWindows(parent,cb,0)
 else:u.EnumWindows(cb,0)
 return out

u.GetWindowRect.argtypes=[w.HWND,c.POINTER(w.RECT)]
u.SetWindowPos.argtypes=[w.HWND,w.HWND,c.c_int,c.c_int,c.c_int,c.c_int,w.UINT]
u.PostMessageW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM]
def rect(h):
 r=w.RECT();u.GetWindowRect(h,c.byref(r));return (r.left,r.top,r.right,r.bottom)
def label(h):
 b=c.create_unicode_buffer(1024);u.GetWindowTextW(h,b,1024);return b.value
def wait(fn):
 for i in range(80):
  if fn():return
  time.sleep(.1)
 raise AssertionError('timeout')

u.SendMessageTimeoutW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM,w.UINT,w.UINT,c.POINTER(c.c_size_t)];u.SendMessageTimeoutW.restype=c.c_ssize_t
u.GetDC.argtypes=[w.HWND];u.GetDC.restype=w.HDC
u.PrintWindow.argtypes=[w.HWND,w.HDC,w.UINT]
u.ReleaseDC.argtypes=[w.HWND,w.HDC]
g=c.WinDLL('gdi32');g.CreateCompatibleDC.argtypes=[w.HDC];g.CreateCompatibleDC.restype=w.HDC
g.CreateCompatibleBitmap.argtypes=[w.HDC,c.c_int,c.c_int];g.CreateCompatibleBitmap.restype=w.HBITMAP
g.SelectObject.argtypes=[w.HDC,w.HGDIOBJ];g.SelectObject.restype=w.HGDIOBJ
g.GetPixel.argtypes=[w.HDC,c.c_int,c.c_int];g.GetPixel.restype=w.DWORD
g.DeleteObject.argtypes=[w.HGDIOBJ];g.DeleteDC.argtypes=[w.HDC]

g.GetDIBits.argtypes=[w.HDC,w.HBITMAP,w.UINT,w.UINT,c.c_void_p,c.c_void_p,w.UINT]
class BI(c.Structure):
 _fields_=[('size',w.DWORD),('width',w.LONG),('height',w.LONG),('planes',w.WORD),('bits',w.WORD),('compression',w.DWORD),('imageSize',w.DWORD),('x',w.LONG),('y',w.LONG),('colors',w.DWORD),('important',w.DWORD)]
def capture(h,path):
 r=w.RECT();u.GetClientRect(h,c.byref(r));dc=u.GetDC(h);mem=g.CreateCompatibleDC(dc);bm=g.CreateCompatibleBitmap(dc,r.right,r.bottom);old=g.SelectObject(mem,bm)
 assert u.PrintWindow(h,mem,3)
 g.SelectObject(mem,old);bi=BI();bi.size=c.sizeof(BI);bi.width=r.right;bi.height=-r.bottom;bi.planes=1;bi.bits=32;data=c.create_string_buffer(r.right*r.bottom*4)
 assert g.GetDIBits(mem,bm,0,r.bottom,data,c.byref(bi),0)
 image=Image.frombytes('RGB',(r.right,r.bottom),data.raw,'raw','BGRX');image.save(path)
 g.DeleteObject(bm);g.DeleteDC(mem);u.ReleaseDC(h,dc);return image
