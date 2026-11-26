"""Compare the exact released binaries on one disposable CI desktop."""
import os,sys,json,time,subprocess,hashlib,random,math,atexit,ctypes as c,ctypes.wintypes as w
from pathlib import Path
from PIL import Image,ImageDraw
from window_test_helpers import u,g,wins,label
if os.getenv('GITHUB_ACTIONS')!='true':raise SystemExit('Isolated CI only; never run on user desktop')
root=Path(__file__).resolve().parents[1];out=root/'build/compare52';out.mkdir(parents=True,exist_ok=True)
fixtures=out/'photos';fixtures.mkdir(exist_ok=True)
colors=[];manifest=[]
for i in range(24):
    color=(30+(i*61)%190,30+(i*97)%190,30+(i*43)%190);colors.append(color)
    rng=random.Random(52000+i)
    im=Image.frombytes('RGB',(1500,1000),rng.randbytes(1500*1000*3)).resize((6000,4000),Image.Resampling.BICUBIC)
    draw=ImageDraw.Draw(im)
    for x,y in [(.25,.25),(.75,.25),(.5,.5),(.25,.75),(.75,.75)]:
        px,py=int(im.width*x),int(im.height*y);draw.rectangle((px-150,py-150,px+150,py+150),fill=color)
    path=fixtures/(f'{i:03}.jpg');im.save(path,quality=95,progressive=i%3==0)
    manifest.append({'file':path.name,'bytes':path.stat().st_size,'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'progressive':i%3==0})
    del im,draw
    print('Fixture',i,manifest[-1]['bytes'],flush=True)
def wait(fn,seconds=20):
    end=time.perf_counter()+seconds
    while time.perf_counter()<end:
        if fn():return time.perf_counter()
        time.sleep(.001)
    raise AssertionError('timeout: '+str([(name,label(handle)) for handle,name in wins(p.pid)]))
def pixel_ready(h,index):
    if f'{index:03}.jpg' not in label(h):return False
    r=w.RECT();u.GetClientRect(h,c.byref(r))
    dc=u.GetDC(h)
    try:
        expected=colors[index]
        # Both binaries must have painted the target, not merely changed the title.
        for px,py in [(.5,.5),(.25,.25),(.75,.75)]:
            scale=min(r.right/6000,r.bottom/4000)
            x=int((r.right-6000*scale)/2+6000*scale*px)
            y=int((r.bottom-4000*scale)/2+4000*scale*py)
            value=g.GetPixel(dc,x,y);actual=(value&255,(value>>8)&255,(value>>16)&255)
            if max(abs(a-b) for a,b in zip(actual,expected))>14:return False
        return True
    finally:u.ReleaseDC(h,dc)
def percentile(xs,p):return sorted(xs)[max(0,math.ceil(len(xs)*p)-1)]
runs=[]
errors=[]
def save_results():
    save_results()
    # (out/'results.json').write_text(json.dumps({'scope':'Exact released fyto binaries and official unlicensed BandiView trial on the same CI desktop, OS cache warm; complex 24MP JPEGs; first correct target pixels, not full-detail, real photos or physical scanout','display':[u.GetSystemMetrics(0),u.GetSystemMetrics(1)],'fixtures':manifest,'runs':runs,'errors':errors,'vendor_version':os.getenv('FYTO_BANDIVIEW_VERSION')},indent=2))
atexit.register(save_results)
def record_error(t,v,tb):
    errors.append(str(v));save_results();sys.__excepthook__(t,v,tb)
sys.excepthook=record_error
for order,version in enumerate(['52','71','bandiview','bandiview','71','52']):
    vendor=version=='bandiview'
    folder=Path(os.environ['FYTO_BANDIVIEW_DIR']) if vendor else root/'build'/('released-'+version)
    candidates=list(folder.rglob('BandiView.exe' if vendor else 'fyto.exe'));assert len(candidates)==1,candidates
    exe=candidates[0]
    # No inherited user settings and the same explicit configuration.
    if not vendor:(exe.parent/'fyto.ini').write_text('[fyto]\nappdata=0\nmultiple_instances=1\ndefaults_version=5\nshow_caption=0\nshow_menu=0\nshow_status=0\nshow_controls=0\nshow_thickframe=0\nmouse_wheel_action=1\nnav_sort=0\nnav_sort_ascending=1\npreload_next=1\ncache_last=1\n',encoding='ascii')
    if not vendor:(exe.parent/'associations.ini').write_text('[fyto]\nSetupVersion=1\n',encoding='ascii')
    samples=[];p=None;h=None;t=time.perf_counter()
    try:
        args=['-fullscreen','-mode:fit2window'] if vendor else ['/fullscreen']
        p=subprocess.Popen([str(exe.resolve()),*args,str((fixtures/'000.jpg').resolve())],cwd=exe.parent)
        wait(lambda:any('000.jpg' in label(handle) for handle,_ in wins(p.pid)))
        h=next(handle for handle,_ in wins(p.pid) if '000.jpg' in label(handle))
        end=wait(lambda:pixel_ready(h,0));startup=(end-t)*1000
        time.sleep(.4);index=0
        def step(target,delay,phase):
            global index
            direction=1 if target>index else -1
            start=time.perf_counter()
            assert u.PostMessageW(h,0x100,0x27 if direction>0 else 0x25,0)
            end=wait(lambda:pixel_ready(h,target))
            samples.append({'phase':phase,'target':target,'ms':(end-start)*1000})
            index=target
            if delay:time.sleep(delay)
        for target in range(1,24):step(target,.12,'first-pass-120ms-pacing')
        for target in range(22,-1,-1):step(target,.12,'reverse-120ms-pacing')
        for repeat in range(12):
            step(1,0,'two-image-cache');step(0,0,'two-image-cache')
        for target in range(1,24):step(target,0,'continuous')
        phases={}
        for phase in sorted({s['phase'] for s in samples}):
            values=[s['ms'] for s in samples if s['phase']==phase]
            phases[phase]={'n':len(values),'p50_ms':percentile(values,.5),'p95_ms':percentile(values,.95),'max_ms':max(values)}
        run={'version':version,'order':order,'startup_ms':startup,'exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'samples':samples,'phases':phases}
        runs.append(run);print(json.dumps({k:v for k,v in run.items() if k!='samples'}),flush=True)
    finally:
        if h:u.PostMessageW(h,0x10,0,0)
        if p:
            try:p.wait(timeout=8)
            except subprocess.TimeoutExpired:p.kill();p.wait()
    (out/'results.json').write_text(json.dumps({'scope':'Exact released binaries, same runner, OS file cache warm, 24 complex 24MP local JPEGs, ABBA order; first correct target pixels, not full-resolution detail or physical scanout','display':[u.GetSystemMetrics(0),u.GetSystemMetrics(1)],'fixtures':manifest,'runs':runs},indent=2))
