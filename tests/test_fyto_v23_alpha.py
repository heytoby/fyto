from window_test_helpers import *
from PIL import ImageDraw,ImageFont
import hashlib
fixtures=Path('work/fyto-alpha-fixtures').resolve();font=ImageFont.truetype('C:/Windows/Fonts/arial.ttf',60)
palette=Image.new('P',(320,200),0);palette.putpalette([0]*768);d=ImageDraw.Draw(palette);d.text((25,60),'fyto',font=font,fill=1);d.rectangle((210,55,275,140),fill=1);palette.save(fixtures/'2-palette.png',transparency=0)
semi=Image.new('RGBA',(320,200),(0,0,0,0));d=ImageDraw.Draw(semi);d.rectangle((96,32,143,63),fill=(0,0,0,128));d.rectangle((96,96,143,127),fill=(255,255,255,128));d.rectangle((192,32,223,63),fill=(30,120,200,255));d.rectangle((192,112,239,159),fill=(0,0,0,255));semi.save(fixtures/'3-semi.png')
Image.new('RGB',(320,200),(0,0,0)).save(fixtures/'4-opaque-rgb.png');Image.new('RGBA',(320,200),(0,0,0,255)).save(fixtures/'5-opaque-rgba.png');Image.new('RGB',(320,200),(30,120,200)).save(fixtures/'6-color.png')
files=sorted(fixtures.glob('[1-6]-*.png'));before={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in files}
root=Path('work/fyto-v23-alpha-check').resolve();root.mkdir(exist_ok=True);shutil.copy2('work/fyto-source/voidImageViewer/vs2019/x64/Release/fyto.exe',root/'fyto.exe');(root/'first-run.complete').write_text('test')
(root/'fyto.ini').write_text('[fyto]\nappdata=0\nmultiple_instances=1\ndefaults_version=2\npreload_next=1\n')
report={'version':'1.0.0.23','cases':[]}
def image_area(h,name):
 im=capture(h,root/name);left=im.width//2-160;top=(im.height-32)//2-100
 return im,im.crop((left,top,left+320,top+200))
def expect_current(h,name):
 wait(lambda:label(h)==name);time.sleep(.2)
 u.PostMessageW(h,0x111,1022,0);time.sleep(.1)
 full,im=image_area(h,name+'.screen.png');assert full.getpixel((2,2))==(28,28,28),(name,'window background',full.getpixel((2,2)))
 if name.startswith(('1-','2-')):
  assert im.getpixel((240,90))==(0,0,0),(name,im.getpixel((240,90)))
  colors={im.getpixel((x,y)) for x,y in [(4,4),(20,4),(4,20),(20,20)]};assert colors=={(216,216,216),(240,240,240)},(name,colors)
  im.save(root/(name+'.visible.png'))
 elif name.startswith('3-'):
  base=im.getpixel((70,40))[0];got=im.getpixel((102,40))[0];assert abs(got-base*127/255)<=2,('half black',base,got)
  base=im.getpixel((70,104))[0];got=im.getpixel((102,104))[0];assert abs(got-(255*128+base*127)/255)<=2,('half white',base,got)
  assert im.getpixel((204,40))==(30,120,200) and im.getpixel((210,130))==(0,0,0)
 elif name.startswith(('4-','5-')):assert im.getextrema()==((0,0),(0,0),(0,0)),(name,im.getextrema())
 else:assert im.getextrema()==((30,30),(120,120),(200,200)),(name,im.getextrema())
 report['cases'].append(name)
p=subprocess.Popen([str(root/'fyto.exe'),str(files[0])],cwd=root)
try:
 wait(lambda:any(n=='fyto' for _,n in wins(p.pid)));h=next(h for h,n in wins(p.pid) if n=='fyto');time.sleep(.4)
 for i,f in enumerate(files):
  if i:u.PostMessageW(h,0x100,39,0)
  expect_current(h,f.name)
 # Return to a transparent image through preloaded navigation, then check fullscreen.
 for f in list(reversed(files))[1:]:u.PostMessageW(h,0x100,37,0);expect_current(h,f.name)
 u.PostMessageW(h,0x111,1024,0);time.sleep(.3);u.PostMessageW(h,0x111,1022,0);time.sleep(.1);full,im=image_area(h,'fullscreen.png');assert full.getpixel((2,2))==(28,28,28)
 assert im.getpixel((240,90))==(0,0,0) and im.getpixel((4,4))[0]>=216
 report['fullscreen']='transparent black text visible; default #1C1C1C outside image'
finally:
 u.PostMessageW(h,0x10,0,0)
 try:p.wait(timeout=5)
 except subprocess.TimeoutExpired:p.terminate()
ini=(root/'fyto.ini').read_text(encoding='utf-8-sig')
for mode in ['windowed','fullscreen']:
 for ch in 'rgb':assert f'{mode}_background_color_{ch}=28' in ini
assert all(hashlib.sha256(f.read_bytes()).hexdigest()==before[f.name] for f in files)
report.update(source_files='unchanged hashes',opaque_pixels='exact RGB retained',alpha='RGBA and indexed PNG transparency, semitransparent black/white verified',default_background='both modes RGB 28,28,28')
Path('work/fyto-v23-alpha-validation.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))
