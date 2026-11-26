"""Compare the exact released binaries on one disposable CI desktop."""
import os,sys,json,time,subprocess,hashlib,random,math,atexit,ctypes as c,ctypes.wintypes as w
from pathlib import Path
from PIL import Image,ImageDraw
from process_resources import process_resources
from window_test_helpers import u,g,wins,label,BI
g.BitBlt.argtypes=[w.HDC,c.c_int,c.c_int,c.c_int,c.c_int,w.HDC,c.c_int,c.c_int,w.DWORD]
g.BitBlt.restype=w.BOOL
if os.getenv('GITHUB_ACTIONS')!='true':raise SystemExit('Isolated CI only; never run on user desktop')
root=Path(__file__).resolve().parents[1];out=root/'build/compare52';out.mkdir(parents=True,exist_ok=True)
fixtures=out/'photos';fixtures.mkdir(exist_ok=True)
colors=[];manifest=[]
for i in range(24):
    color=(40+(i*61)%180,70+(i*97)%150,40+(i*43)%190)
    if max(color)-min(color)<50:color=(240,color[1],40)
    colors.append(color)
    rng=random.Random(52000+i)
    im=Image.frombytes('RGB',(1500,1000),rng.randbytes(1500*1000*3)).resize((6000,4000),Image.Resampling.BICUBIC)
    draw=ImageDraw.Draw(im)
    for x,y in [(.25,.25),(.75,.25),(.5,.5),(.25,.75),(.75,.75)]:
        px,py=int(im.width*x),int(im.height*y);draw.rectangle((px-150,py-150,px+150,py+150),fill=color)
    path=fixtures/(f'{i:03}.jpg');im.save(path,quality=95,progressive=i%3==0)
    manifest.append({'file':path.name,'bytes':path.stat().st_size,'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'progressive':i%3==0})
    del im,draw
    print('Fixture',i,manifest[-1]['bytes'],flush=True)
probe=root/'build/jpeg-tests/probe_jpeg_stages.exe'
if probe.exists():
    result=subprocess.run([str(probe),*[str(fixtures/f'{i:03}.jpg') for i in range(3)]],capture_output=True,text=True,check=True)
    print(result.stdout,flush=True)
    (out/'jpeg-stages.txt').write_text(result.stdout)
def wait(fn,seconds=20):
    end=time.perf_counter()+seconds
    while time.perf_counter()<end:
        if fn():return time.perf_counter()
        time.sleep(.001)
    raise AssertionError('timeout: '+str([(name,label(handle)) for handle,name in wins(p.pid)]))
def pixel_ready(h,index):
    if f'{index:03}.jpg' not in label(h):return False
    r=w.RECT();u.GetClientRect(h,c.byref(r))
    origin=w.POINT(0,0);assert u.ClientToScreen(h,c.byref(origin))
    dc=u.GetDC(None);mem=g.CreateCompatibleDC(dc)
    bitmap=g.CreateCompatibleBitmap(dc,1,r.bottom);old=g.SelectObject(mem,bitmap)
    try:
        assert g.BitBlt(mem,0,0,1,r.bottom,dc,origin.x+r.right//2,origin.y,0x00CC0020)
        g.SelectObject(mem,old)
        bi=BI();bi.size=c.sizeof(BI);bi.width=1;bi.height=-r.bottom;bi.planes=1;bi.bits=32
        pixels=(c.c_ubyte*(r.bottom*4))()
        assert g.GetDIBits(mem,bitmap,0,r.bottom,pixels,c.byref(bi),0)
        expected=colors[index];matches=0
        for i in range(8,57):
            offset=int(r.bottom*i/64)*4
            actual=(pixels[offset+2],pixels[offset+1],pixels[offset])
            if max(abs(a-b) for a,b in zip(actual,expected))<=14:
                matches+=1
                if matches>=3:return True
        return False
    finally:
        g.SelectObject(mem,old);g.DeleteObject(bitmap);g.DeleteDC(mem);u.ReleaseDC(None,dc)
def percentile(xs,p):return sorted(xs)[max(0,math.ceil(len(xs)*p)-1)]
runs=[]
errors=[]
def save_results():
    (out/'results.json').write_text(json.dumps({'scope':'Exact released fyto binaries and official unlicensed BandiView trial on the same CI desktop, OS cache warm; complex 24MP JPEGs; composited-screen correct target pixels, not full-detail, real photos or physical scanout','display':[u.GetSystemMetrics(0),u.GetSystemMetrics(1)],'fixtures':manifest,'runs':runs,'errors':errors,'vendor_version':os.getenv('FYTO_BANDIVIEW_VERSION')},indent=2))
atexit.register(save_results)
def record_error(t,v,tb):
    errors.append(str(v));save_results();sys.__excepthook__(t,v,tb)
sys.excepthook=record_error
for order,version in enumerate(['52','71','bandiview','72','72','71','52']):
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
        run['resources']=process_resources(p.pid)
        runs.append(run);print(json.dumps({k:v for k,v in run.items() if k!='samples'}),flush=True)
    finally:
        if h:u.PostMessageW(h,0x10,0,0)
        if p:
            try:p.wait(timeout=8)
            except subprocess.TimeoutExpired:p.kill();p.wait()
    (out/'results.json').write_text(json.dumps({'scope':'Exact released binaries, same runner, OS file cache warm, 24 complex 24MP local JPEGs, ABBA order; first correct target pixels, not full-resolution detail or physical scanout','display':[u.GetSystemMetrics(0),u.GetSystemMetrics(1)],'fixtures':manifest,'runs':runs},indent=2))

# Gates cover revisiting this working set; first-seen decode is disclosed separately.
candidate=[r for r in runs if r['version']=='72']
baseline=[r for r in runs if r['version']=='71']
vendor=[r for r in runs if r['version']=='bandiview']
assert len(candidate)==2 and len(baseline)==2 and len(vendor)==1
for phase in ['reverse-120ms-pacing','continuous']:
    current=max(r['phases'][phase]['p95_ms'] for r in candidate)
    old=max(r['phases'][phase]['p95_ms'] for r in baseline)
    reference=max(r['phases'][phase]['p95_ms'] for r in vendor)
    assert current<=old*.70,(phase,'regression versus 71',current,old)
    assert current<=reference*1.2+20,(phase,'vendor gap persists',current,reference)
for run in candidate:
    assert run['resources']['private_commit_peak_MiB']<=256,run['resources']
print('Candidate working-set performance and native memory gates passed',flush=True)
