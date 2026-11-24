"""Broader native-viewer regression. Runs only on the disposable CI desktop."""
from pathlib import Path
import os,sys,time,subprocess,shutil,json,traceback,hashlib,ctypes as c,ctypes.wintypes as w
from PIL import Image,ImageDraw,ImageCms,ImageOps
from window_test_helpers import u,g,wins,label,capture
from process_resources import process_resources
import random,msvcrt
from io import BytesIO
if os.getenv('GITHUB_ACTIONS')!='true':
    raise SystemExit('Requires an isolated GitHub Actions runner')
root=Path(__file__).resolve().parents[1]
out=root/'build/extended-audit';out.mkdir(parents=True,exist_ok=True)
viewer=out/'fyto.exe';shutil.copy2(root/'voidImageViewer/vs2019/x64/Release/fyto.exe',viewer)
(out/'fyto.ini').write_text('[fyto]\nappdata=0\nmultiple_instances=1\ndefaults_version=5\nshow_caption=0\nshow_menu=0\nshow_status=0\nshow_controls=0\nshow_thickframe=0\nmouse_wheel_action=1\nsort=0\nsort_ascending=1\n',encoding='ascii')
(out/'associations.ini').write_text('[fyto]\nSetupVersion=1\n',encoding='ascii')
header=(root/'voidImageViewer/src/viv.h').read_text(encoding='utf8')
start=header.index('VIV_ID_EDIT_CUT = 1000');block=header[start:header.index('};',start)]
ids={};number=999
for item in block.split(','):
    item=item.strip()
    if not item:continue
    if '=' in item:name,value=item.split('=',1);number=int(value.strip(),0)
    else:name=item;number+=1
    ids[name.strip()]=number
results=[];processes=[]
def wait(fn,timeout=12):
    deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
        if fn():return
        time.sleep(.015)
    raise AssertionError('condition timed out')
def command(hwnd,name):
    assert u.PostMessageW(hwnd,0x111,ids[name],0)
def launch(path):
    p=subprocess.Popen([str(viewer.resolve()),'/window',str(path.resolve())],cwd=out);processes.append(p)
    wait(lambda:any(n=='fyto' for _,n in wins(p.pid)))
    h=next(h for h,n in wins(p.pid) if n=='fyto')
    wait(lambda:label(h)==path.name);return p,h
def close(p,h):
    u.PostMessageW(h,0x10,0,0)
    p.wait(timeout=8);assert p.returncode==0,p.returncode
def center(h):
    r=w.RECT();assert u.GetClientRect(h,c.byref(r))
    dc=u.GetDC(h);value=g.GetPixel(dc,r.right//2,r.bottom//2);u.ReleaseDC(h,dc)
    return (value&255,(value>>8)&255,(value>>16)&255)
def near(a,b,tolerance=10):return max(abs(x-y) for x,y in zip(a,b))<=tolerance
def await_color(h,color):
    wait(lambda:near(center(h),color))
def check(name,fn):
    try:
        detail=fn();results.append({'name':name,'pass':True,'detail':detail});print(name,'PASS',flush=True)
    except BaseException:
        error=traceback.format_exc();results.append({'name':name,'pass':False,'error':error});print(name,'FAIL',error,flush=True)
    finally:
        for p in processes:
            if p.poll() is None:
                for h,n in wins(p.pid):
                    if n=='fyto':u.PostMessageW(h,0x10,0,0)
                try:p.wait(timeout=5)
                except subprocess.TimeoutExpired:p.kill();p.wait()
def patterned(size):
    im=Image.new('RGB',size,(160,70,110));d=ImageDraw.Draw(im)
    for box,color in [((0,0,size[0]//2,size[1]//2),(230,40,60)),((size[0]//2,0,size[0],size[1]//2),(30,210,70)),((0,size[1]//2,size[0]//2,size[1]),(50,60,230)),((size[0]//2,size[1]//2,size[0],size[1]),(210,190,40))]:
        d.rectangle(box,fill=color)
    return im
base=patterned((2400,1600));fixtures=[]
def save(name,image,**kwargs):
    folder=out/name;folder.mkdir()
    path=folder/name;image.save(path,**kwargs);fixtures.append(path)
save('rgb.png',base)
save('srgb.png',base,icc_profile=ImageCms.ImageCmsProfile(ImageCms.createProfile('sRGB')).tobytes())
alpha=base.convert('RGBA');alpha.putalpha(128);save('alpha.png',alpha)
save('rgb.bmp',base);save('lossless.webp',base,lossless=True);save('lossy.webp',base,quality=95)
save('static.gif',base);save('rgb.tiff',base,compression='tiff_lzw')
save('gray.tiff',base.convert('L'),compression='raw')
save('cmyk.jpg',base.convert('CMYK'),quality=95);save('gray.jpg',base.convert('L'),quality=95)
save('srgb.jpg',base,quality=95,icc_profile=ImageCms.ImageCmsProfile(ImageCms.createProfile('sRGB')).tobytes())
save('icon.ico',base.resize((256,256)),sizes=[(16,16),(32,32),(64,64),(256,256)])
save('alpha.ico',alpha.resize((256,256)),sizes=[(32,32),(256,256)])
for mp,size in [(12,(4000,3000)),(24,(6000,4000)),(48,(8000,6000))]:
    save(str(mp)+'mp.jpg',patterned(size),quality=95,progressive=True)
large=patterned((6000,4000))
ImageDraw.Draw(large).rectangle((2744,1744,3256,2256),fill=(40,180,90))
save('24mp-rgb.png',large)
large_alpha=large.convert('RGBA');large_alpha.putalpha(128);save('24mp-alpha.png',large_alpha)
ImageDraw.Draw(large).rectangle((2744,1744,3256,2256),fill=(40,90,200))
save('24mp-rgb.tiff',large,compression='tiff_lzw')
save('24mp-gray.tiff',large.convert('L'),compression='raw')
save('24mp-cmyk.jpg',large.convert('CMYK'),quality=95)
for profile in sorted((root/'tests/icc-profiles').glob('*.icc')):
    save('24mp-'+profile.stem+'.jpg',large,quality=95,icc_profile=profile.read_bytes())
noisy=Image.frombytes('RGB',large.size,random.Random(71).randbytes(6000*4000*3));draw=ImageDraw.Draw(noisy)
for px,py in [(.25,.25),(.75,.25),(.25,.75),(.75,.75)]:
    x,y=int(6000*px),int(4000*py);draw.rectangle((x-256,y-256,x+256,y+256),fill=large.getpixel((x,y)))
draw.rectangle((2744,1744,3256,2256),fill=(170,90,160))
save('24mp-noise.png',noisy)
del noisy,draw,large_alpha,large
padded_dir=out/'padded-input';padded_dir.mkdir()
padded=padded_dir/'padded-input.jpg'
shutil.copy2(next(p for p in fixtures if p.name=='24mp.jpg'),padded)
kernel=c.WinDLL('kernel32',use_last_error=True)
kernel.DeviceIoControl.argtypes=[w.HANDLE,w.DWORD,c.c_void_p,w.DWORD,c.c_void_p,w.DWORD,c.POINTER(w.DWORD),c.c_void_p]
with padded.open('r+b') as f:
    returned=w.DWORD()
    assert kernel.DeviceIoControl(msvcrt.get_osfhandle(f.fileno()),0x900c4,None,0,None,0,c.byref(returned),None),'sparse fixture setup failed'
    f.truncate(320*1024**2)
fixtures.append(padded)
def file_hash(path):
    digest=hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda:f.read(1024*1024),b''):digest.update(chunk)
    return digest.hexdigest()
def budget(p,pixels):
    info=process_resources(p.pid);info['budget_applied']=pixels<=24000000
    if info['budget_applied']:assert info['private_commit_peak_MiB']<=256,info
    return info

def format_case(path):
    p,h=launch(path);reference=ImageOps.exif_transpose(Image.open(path))
    reference_engine='Pillow decoded pixels'
    embedded=reference.info.get('icc_profile')
    if embedded and reference.mode in ['RGB','RGBA']:
        flags=ImageCms.Flags.NOOPTIMIZE if hasattr(ImageCms,'Flags') else ImageCms.FLAGS['NOTPRECALC']
        alpha_channel=reference.getchannel('A') if reference.mode=='RGBA' else None
        reference=ImageCms.profileToProfile(reference.convert('RGB'),ImageCms.ImageCmsProfile(BytesIO(embedded)),
            ImageCms.createProfile('sRGB'),renderingIntent=1,outputMode='RGB',flags=flags)
        if alpha_channel is not None:reference.putalpha(alpha_channel)
        reference_engine='LittleCMS relative-colorimetric reference'
    if reference.mode=='RGBA':
        # Transparency is deliberately shown on a 16 px gray checkerboard.
        bg=Image.new('RGBA',reference.size,(240,240,240,255));draw=ImageDraw.Draw(bg)
        for y in range(0,bg.height,16):
            for x in range(0,bg.width,16):
                if ((x//16)^(y//16))&1:draw.rectangle((x,y,x+15,y+15),fill=(216,216,216,255))
        bg.alpha_composite(reference);reference=bg.convert('RGB')
    elif reference.mode=='CMYK':
        # Unprofiled CMYK has no unique sRGB interpretation. Independently
        # compare the fallback against Windows GDI+'s color-managed decoding,
        # rather than imposing Pillow's device-independent arithmetic mapping.
        ps_script="""
Add-Type -AssemblyName System.Drawing
$bmp=[System.Drawing.Bitmap]::new($env:FYTO_REFERENCE_FILE,$true)
try {
    $colors=@()
    foreach($point in @(@(0.25,0.25),@(0.75,0.25),@(0.25,0.75),@(0.75,0.75))) {
        $pixel=$bmp.GetPixel([int]($bmp.Width*$point[0]),[int]($bmp.Height*$point[1]))
        $colors+=,@([int]$pixel.R,[int]$pixel.G,[int]$pixel.B)
    }
    ConvertTo-Json -Compress -InputObject $colors
} finally {$bmp.Dispose()}
"""
        native=subprocess.run(['powershell.exe','-NoProfile','-NonInteractive','-Command',ps_script],
            env=dict(os.environ,FYTO_REFERENCE_FILE=str(path.resolve())),capture_output=True,text=True,check=True,timeout=20)
        samples=json.loads(native.stdout.strip());reference=Image.new('RGB',reference.size)
        draw=ImageDraw.Draw(reference);rw,rh=reference.size
        for i,color in enumerate(samples):
            x=(i%2)*rw//2;y=(i//2)*rh//2
            draw.rectangle((x,y,x+rw//2-1,y+rh//2-1),fill=tuple(color))
        reference_engine='Windows GDI+ color-managed CMYK reference'
    else:reference=reference.convert('RGB')
    # Wait for a known reference pixel; a title or non-black background is insufficient.
    def reference_ready():
        r=w.RECT();assert u.GetClientRect(h,c.byref(r))
        rw,rh=reference.size;scale=min(r.right/rw,r.bottom/rh)
        x=int((r.right-rw*scale)/2+rw*scale*.25)
        y=int((r.bottom-rh*scale)/2+rh*scale*.25)
        dc=u.GetDC(h);value=g.GetPixel(dc,x,y);u.ReleaseDC(h,dc)
        actual=(value&255,(value>>8)&255,(value>>16)&255)
        return near(actual,reference.getpixel((rw//4,rh//4)),14)
    wait(reference_ready);time.sleep(.05)
    im=capture(h,out/(path.name+'-render.png'));cw,ch=im.size
    rw,rh=reference.size;scale=min(cw/rw,ch/rh);dw,dh=rw*scale,rh*scale
    comparisons=[]
    for px,py in [(.25,.25),(.75,.25),(.25,.75),(.75,.75)]:
        actual=im.getpixel((int((cw-dw)/2+dw*px),int((ch-dh)/2+dh*py)))
        expected=reference.getpixel((int(rw*px),int(rh*py)))
        assert near(actual,expected,14),(path.name,actual,expected,im.size)
        comparisons.append([list(actual),list(expected)])
    resources=budget(p,rw*rh)
    close(p,h)
    return {'sha256':file_hash(path),'file_bytes':path.stat().st_size,'reference_size':[rw,rh],'window_client':[cw,ch],
            'pixels':comparisons,'reference_engine':reference_engine,'resources':resources}
for path in fixtures:check('format:'+path.name,lambda path=path:format_case(path))
def animation(ext):
    folder=out/('animated-'+ext);folder.mkdir()
    path=folder/('animation.'+ext);colors=[(220,40,60),(40,210,80)]
    frames=[Image.new('RGB',(1200,800),x) for x in colors]
    frames[0].save(path,save_all=True,append_images=frames[1:],duration=150,loop=0,lossless=True)
    p,h=launch(path);seen=set();deadline=time.monotonic()+3
    while time.monotonic()<deadline:
        pixel=center(h)
        for i,color in enumerate(colors):
            if near(pixel,color,15):seen.add(i)
        if len(seen)==2:break
        time.sleep(.03)
    assert len(seen)==2,(ext,seen,center(h));close(p,h);return {'frames_seen':sorted(seen)}
for ext in ['gif','webp']:check('animation:'+ext,lambda ext=ext:animation(ext))
def overwritten(preserve_metadata=False):
    folder=out/('overwrite-preserve' if preserve_metadata else 'overwrite');folder.mkdir()
    a=folder/'00.bmp';b=folder/'01.bmp'
    Image.new('RGB',(1200,800),(210,40,60)).save(a);Image.new('RGB',(1200,800),(40,70,210)).save(b)
    p,h=launch(a);await_color(h,(210,40,60))
    command(h,'VIV_ID_NAV_NEXT');wait(lambda:label(h)==b.name);await_color(h,(40,70,210))
    before=a.stat();replacement=folder/'replace.tmp'
    Image.new('RGB',(1200,800),(40,210,70)).save(replacement,format='BMP')
    if preserve_metadata:os.utime(replacement,ns=(before.st_atime_ns,before.st_mtime_ns))
    os.replace(replacement,a)
    # The watcher must discard prior directory and rendered-image snapshots.
    time.sleep(.2);command(h,'VIV_ID_NAV_PREV');wait(lambda:label(h)==a.name)
    try:await_color(h,(40,210,70))
    except AssertionError:raise AssertionError({'title':label(h),'actual_center':center(h),'expected':[40,210,70]})
    close(p,h);return {'same_size':a.stat().st_size==before.st_size,'same_mtime':a.stat().st_mtime_ns==before.st_mtime_ns}
check('overwrite:normal',overwritten)
check('overwrite:same-size-time-replaced-file',lambda:overwritten(True))
def directory_case(count):
    folder=out/('directory-'+str(count));folder.mkdir()
    data=Image.new('RGB',(800,600),(150,80,190))
    from io import BytesIO
    buffer=BytesIO();data.save(buffer,format='PNG');payload=buffer.getvalue()
    for i in range(count):(folder/(str(i).zfill(5)+'.png')).write_bytes(payload)
    p,h=launch(folder/'00000.png');await_color(h,(150,80,190))
    command(h,'VIV_ID_NAV_END');wait(lambda:label(h)==str(count-1).zfill(5)+'.png',30)
    command(h,'VIV_ID_NAV_HOME');wait(lambda:label(h)=='00000.png',30)
    (folder/'00001.png').unlink();new=folder/'00001a.png';new.write_bytes(payload)
    time.sleep(.2);command(h,'VIV_ID_NAV_NEXT')
    try:wait(lambda:label(h)=='00001a.png',30)
    except AssertionError:raise AssertionError({'actual_title':label(h),'expected':'00001a.png','entries':count})
    close(p,h);return {'entries':count,'end_home_and_remove_add':True}
for count in [10,1000,10000]:check('directory:'+str(count),lambda count=count:directory_case(count))
def mixed_large_navigation():
    folder=out/'mixed-large';folder.mkdir()
    choices=['24mp-noise.png','24mp-rgb.png','24mp-rgb.tiff']
    colors=[(170,90,160),(40,180,90),(40,90,200)];targets=[]
    for i,name in enumerate(choices):
        source=next(p for p in fixtures if p.name==name)
        target=folder/(str(i).zfill(2)+source.suffix);os.link(source,target);targets.append(target)
    p,h=launch(targets[0]);await_color(h,colors[0]);resources=[budget(p,24000000)]
    for iteration in range(18):
        index=(iteration+1)%len(targets);command(h,'VIV_ID_NAV_NEXT')
        wait(lambda:label(h)==targets[index].name,30)
        try:await_color(h,colors[index])
        except AssertionError:raise AssertionError({'iteration':iteration,'title':label(h),'actual_center':center(h),'expected':colors[index],'resources':process_resources(p.pid)})
        resources.append(budget(p,24000000))
    close(p,h);return {'transitions':18,'resources':resources}
check('mixed-24mp-memory',mixed_large_navigation)

report={'commit':os.getenv('GITHUB_SHA'),'scope':'Disposable CI desktop. Generated fixtures and application pixels, not full release acceptance.','results':results}
(out/'results.json').write_text(json.dumps(report,indent=2),encoding='utf8')
print(json.dumps({'passed':sum(x['pass'] for x in results),'total':len(results),'failures':[x['name'] for x in results if not x['pass']]},indent=2))
if not all(x['pass'] for x in results):raise SystemExit(1)
