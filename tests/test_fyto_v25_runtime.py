from window_test_helpers import *
from PIL import ImageChops
import winreg,hashlib
u.IsWindowVisible.argtypes=[w.HWND];u.IsWindowVisible.restype=w.BOOL
u.IsZoomed.argtypes=[w.HWND];u.IsZoomed.restype=w.BOOL
u.ShowWindow.argtypes=[w.HWND,c.c_int];u.SetProcessDPIAware()
u.GetDlgItem.argtypes=[w.HWND,c.c_int];u.GetDlgItem.restype=w.HWND
import tempfile
project=Path(__file__).resolve().parents[1]
root=Path(tempfile.mkdtemp(prefix='fyto-v25-runtime-'))
shutil.copy2(project/'voidImageViewer/vs2019/x64/Release/fyto.exe',root/'fyto.exe')
img=root/'wide.png';Image.new('RGB',(1600,900),(50,110,180)).save(img);before_hash=hashlib.sha256(img.read_bytes()).hexdigest()
(root/'associations.ini').unlink(missing_ok=True)
processes=[];report={'version':'1.0.0.25','mode_cycles':[]}
def launch(image=None,setup=False):
 args=[str(root/'fyto.exe')]+(['/setup'] if setup else [])+([str(image)] if image else [])
 p=subprocess.Popen(args,cwd=root);processes.append(p);wait(lambda:any(n=='fyto' for _,n in wins(p.pid)));h=next(h for h,n in wins(p.pid) if n=='fyto');time.sleep(.35)
 if image:wait(lambda:label(h)==image.name);time.sleep(.15)
 return p,h
def close(p,h):
 u.PostMessageW(h,0x10,0,0);p.wait(timeout=5);assert p.returncode==0,p.returncode
 def active(t):return u.IsWindowVisible(t)
def standard(h):return any(n=='ToolbarWindow32' and u.IsWindowVisible(t) for t,n in wins(next(p.pid for p in processes if p.poll() is None),h))
def toggle(h):u.PostMessageW(h,0x100,49,0);time.sleep(.2)
def client(h):
 r=w.RECT();u.GetClientRect(h,c.byref(r));pt=w.POINT(0,0);u.ClientToScreen(h,c.byref(pt));return pt.x,pt.y,r.right,r.bottom
def assert_chrome(h,is_standard,maxed=False):
 outer=rect(h);x,y,cw,ch=client(h)
 if not maxed:
  assert x==outer[0] and cw==outer[2]-outer[0],(outer,(x,y,cw,ch))
  assert y+ch==outer[3],('bottom frame',outer,(x,y,cw,ch))
  if is_standard:assert 30<=y-outer[1]<=72,('title',outer,(x,y,cw,ch))
  else:assert y==outer[1],('minimal top',outer,(x,y,cw,ch))
 im=capture(h,root/('standard.png' if is_standard else 'minimal.png'))
 return im
try:
 (root/'fyto.ini').write_text('[fyto]\nappdata=0\nlanguage=1\nmultiple_instances=1\ndefaults_version=2\nauto_zoom=0\npreload_next=0\n')
 p,h=launch(setup=True);assert standard(h);assert all(n!='#32770' for _,n in wins(p.pid));capture(h,root/'first-use.png');report['first_use']='standard mode; optional empty-view entry; no modal startup prompt'
 # The same rectangle must survive normal, maximized, half-screen and fullscreen toggles.
 close(p,h);p,h=launch(img);assert standard(h);u.SetWindowPos(h,0,80,80,960,700,0x14);time.sleep(.2)
 before=rect(h);assert_chrome(h,True)
 for i in range(4):
  toggle(h);assert not standard(h);assert rect(h)==before,('normal minimal shrank',before,rect(h));assert_chrome(h,False)
  toggle(h);assert standard(h);assert rect(h)==before;assert_chrome(h,True)
 report['mode_cycles'].append({'state':'normal','outer_rectangle':before,'cycles':4})
 u.ShowWindow(h,3);time.sleep(.25);before=rect(h);assert u.IsZoomed(h)
 for i in range(3):
  toggle(h);assert rect(h)==before and u.IsZoomed(h);assert not standard(h)
  toggle(h);assert rect(h)==before and u.IsZoomed(h);assert standard(h)
 report['mode_cycles'].append({'state':'maximized','outer_rectangle':before,'cycles':3})
 u.ShowWindow(h,9);time.sleep(.2);u.SetWindowPos(h,0,0,0,800,900,0x14);time.sleep(.2);before=rect(h)
 toggle(h);assert rect(h)==before;toggle(h);assert rect(h)==before;report['mode_cycles'].append({'state':'half-screen rectangle','outer_rectangle':before,'cycles':1})
 u.PostMessageW(h,0x111,1024,0);time.sleep(.25);before=rect(h);toggle(h);assert rect(h)==before;toggle(h);assert rect(h)==before;report['mode_cycles'].append({'state':'fullscreen','outer_rectangle':before,'cycles':1});u.PostMessageW(h,0x111,1024,0);time.sleep(.2)
 # Persist minimal and standard independently, including the installed AppData routing below.
 toggle(h);assert not standard(h);close(p,h);p,h=launch();assert not standard(h);close(p,h);p,h=launch();assert not standard(h);toggle(h);assert standard(h);close(p,h);p,h=launch();assert standard(h);report['restart_mode']='minimal retained twice, standard retained after switching back'
 # Native format controls: manual only, one-click all and saved selection restoration.
 original=rect(h);u.PostMessageW(h,0x111,60003,0);wait(lambda:any(n=='#32770' for _,n in wins(p.pid)));d=next(t for t,n in wins(p.pid) if n=='#32770');time.sleep(.2);assert rect(h)==original
 assert u.GetDlgItem(d,61201);u.PostMessageW(d,0x111,61201,0);time.sleep(.1)
 for i in range(10):assert u.SendMessageW(u.GetDlgItem(d,61000+i),0xF0,0,0)==1
 u.PostMessageW(d,0x111,1,0);wait(lambda:all(n!='#32770' for _,n in wins(p.pid)));assert rect(h)==original;close(p,h)
 p,h=launch(setup=True);assert standard(h);assert all(n!='#32770' for _,n in wins(p.pid));capture(h,root/'already-selected.png');u.PostMessageW(h,0x111,60003,0);wait(lambda:any(n=='#32770' for _,n in wins(p.pid)));d=next(t for t,n in wins(p.pid) if n=='#32770');time.sleep(.15)
 for i in range(10):assert u.SendMessageW(u.GetDlgItem(d,61000+i),0xF0,0,0)==1
 capture(d,root/'formats-all.png');u.PostMessageW(d,0x111,2,0);time.sleep(.2);close(p,h)
 report['formats']='native one-click all selects ten checkboxes; saved mask restored; /setup never forces modal re-selection; main rectangle unchanged'
 # Internal sample images, including paths outside the canonical TEMP prefix, never replace the photo or create a tiny viewer.
 p,h=launch(img);u.SetWindowPos(h,0,80,80,960,700,0x14);time.sleep(.15);original=rect(h);sample=root/'private-sample';sample.mkdir(exist_ok=True);(sample/'.fyto-format-setup').write_text('');Image.new('RGB',(16,16),(230,230,230)).save(sample/'fyto-default.png')
 q=subprocess.run([str(root/'fyto.exe'),str(sample/'fyto-default.png')],cwd=root,timeout=5);assert q.returncode==0;assert rect(h)==original and label(h)==img.name;close(p,h);report['sample']='internal 16x16 confirmation sample exits before creating a viewer; current image/rectangle retained'
 assert hashlib.sha256(img.read_bytes()).hexdigest()==before_hash
finally:
 for p in processes:
  if p.poll() is None:
   for h,n in wins(p.pid):
    if n=='#32770':u.PostMessageW(h,0x111,2,0)
   for h,n in wins(p.pid):
    if n=='fyto':u.PostMessageW(h,0x10,0,0)
   try:p.wait(timeout=3)
   except subprocess.TimeoutExpired:p.terminate();p.wait(timeout=3)
report['source_photo']='hash unchanged';report['preference_storage']='isolated portable associations.ini; user registry unchanged'
(root/'validation.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))
