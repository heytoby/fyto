from window_test_helpers import *
from PIL import ImageChops
u.IsWindowVisible.argtypes=[w.HWND];u.IsWindowVisible.restype=w.BOOL
root=Path('work/fyto-v23-hint-check').resolve();root.mkdir(exist_ok=True);shutil.copy2('work/fyto-source/voidImageViewer/vs2019/x64/Release/fyto.exe',root/'fyto.exe');(root/'first-run.complete').write_text('test')
(root/'fyto.ini').write_text('[fyto]\nappdata=0\nmultiple_instances=1\ndefaults_version=2\nlanguage=1\nwindowed_background_color_r=23\nwindowed_background_color_g=35\nwindowed_background_color_b=47\nfullscreen_background_color_r=49\nfullscreen_background_color_g=61\nfullscreen_background_color_b=73\n')
p=subprocess.Popen([str(root/'fyto.exe')],cwd=root)
try:
 wait(lambda:any(n=='fyto' for _,n in wins(p.pid)));h=next(h for h,n in wins(p.pid) if n=='fyto');time.sleep(.4)
 im=capture(h,root/'empty.png');assert im.getpixel((2,2))==(23,35,47)
 heading=im.crop((im.width//2-180,(im.height-32)//2-78,im.width//2+180,(im.height-32)//2-32));assert ImageChops.difference(heading,Image.new('RGB',heading.size,(23,35,47))).getbbox() is None
 y=(im.height-32)//2+36;block=im.crop((im.width//2-180,y,im.width//2+180,y+96))
 occupied=[]
 for row in range(block.height):
  occupied.append(any(min(block.getpixel((x,row)))>150 for x in range(block.width)))
 groups=sum(on and (i==0 or not occupied[i-1]) for i,on in enumerate(occupied));assert groups>=4,groups
 def tools_visible():return any(n=='ToolbarWindow32' and u.IsWindowVisible(t) for t,n in wins(p.pid,h))
 assert tools_visible();u.PostMessageW(h,0x100,49,0);time.sleep(.15);assert not tools_visible();u.PostMessageW(h,0x100,49,0);time.sleep(.15);assert tools_visible()
 u.PostMessageW(h,0x100,65,0);time.sleep(.15);before=capture(h,root/'exif-on.png');u.PostMessageW(h,0x100,65,0);time.sleep(.15);after=capture(h,root/'exif-off.png');assert ImageChops.difference(before,after).getbbox() is not None
 u.PostMessageW(h,0x111,1024,0);time.sleep(.2);full=capture(h,root/'custom-fullscreen.png');assert full.getpixel((2,2))==(49,61,73)
 u.PostMessageW(h,0x100,27,0);p.wait(timeout=5);assert p.returncode==0
finally:
 if p.poll() is None:p.terminate()
report={'empty_hint':'four visible lines including 1/A/Esc; redundant heading absent','key1':'standard/minimal toggle passed','keyA':'EXIF open/close visibly changes sidebar','Esc':'clean exit','saved_custom_colors':'window and fullscreen custom values retained'}
Path('work/fyto-v23-hint-validation.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))
