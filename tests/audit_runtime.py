"""Run only on the isolated CI runner. Record rendered-frame timings, not titles.
QPC paint completion measures application rendering, not physical display scanout.
"""
from pathlib import Path
import ctypes as c,ctypes.wintypes as w
import os,sys,time,subprocess,shutil,json,hashlib,platform,math
from PIL import Image,ImageDraw
from window_test_helpers import u,g,wins,wait

root=Path(__file__).resolve().parents[1]
out=root/'build/audit-runtime';out.mkdir(parents=True,exist_ok=True)
scratch=out/'instance';scratch.mkdir(exist_ok=True)
source=root/'voidImageViewer/vs2019/x64/Release/fyto.exe'
viewer=scratch/'fyto.exe';shutil.copy2(source,viewer)
colors=[(210,30,40),(30,200,50),(40,50,210),(210,180,30),(170,40,190),(30,180,190),(200,100,40),(110,190,50)]
fixtures=[]
corner_colors=[(250,70,20),(30,240,90),(70,30,240),(240,210,50)]
corner_map=[[0,1,2,3],[1,0,3,2],[3,2,1,0],[2,3,0,1],[0,2,1,3],[2,0,3,1],[3,1,2,0],[1,3,0,2]]
for i,color in enumerate(colors):
    size=(6000,4000) if i%2==0 else (4000,6000)
    image=Image.new('RGB',size,color);draw=ImageDraw.Draw(image)
    for x in range(0,size[0],64):draw.line((x,0,x,size[1]//5),fill=(x%255,70,130),width=8)
    for quadrant,color2 in enumerate(corner_colors):
        left=0 if quadrant%2==0 else size[0]*2//3
        top=0 if quadrant<2 else size[1]*2//3
        draw.rectangle((left,top,left+size[0]//3,top+size[1]//3),fill=color2)
    exif=Image.Exif();exif[274]=i+1
    path=scratch/f'{i:02}.jpg';image.save(path,quality=95,progressive=i%3==0,exif=exif)
    fixtures.append({'name':path.name,'size':size,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
(scratch/'fyto.ini').write_text('[fyto]\nappdata=0\nmultiple_instances=1\ndefaults_version=5\nshow_caption=0\nshow_menu=0\nshow_status=0\nmouse_wheel_action=1\nnav_sort=0\nnav_sort_ascending=1\n',encoding='ascii')
(scratch/'associations.ini').write_text('[fyto]\nSelectedFormats=15\nSetupVersion=1\n',encoding='ascii')
trace=out/'messages.tsv'
env=dict(os.environ,FYTO_AUDIT_TRACE=str(trace.resolve()))
frequency=c.c_longlong();c.windll.kernel32.QueryPerformanceFrequency(c.byref(frequency));freq=frequency.value
class Counters(c.Structure):
    _fields_=[('cb',w.DWORD),('faults',w.DWORD)]+[(n,c.c_size_t) for n in ['peak_ws','ws','peak_paged','paged','peak_nonpaged','nonpaged','pagefile','peak_pagefile','private']]
k=c.WinDLL('kernel32');ps=c.WinDLL('psapi')
k.OpenProcess.argtypes=[w.DWORD,w.BOOL,w.DWORD];k.OpenProcess.restype=w.HANDLE
k.CloseHandle.argtypes=[w.HANDLE]
ps.GetProcessMemoryInfo.argtypes=[w.HANDLE,c.POINTER(Counters),w.DWORD]
u.GetGuiResources.argtypes=[w.HANDLE,w.DWORD];u.GetGuiResources.restype=w.DWORD
records=[];samples=[];memory=[];checkpoints=[];offset=0;tail=b''
def read_trace():
    global offset,tail
    if not trace.exists():return
    with trace.open('rb') as f:f.seek(offset);data=f.read();offset=f.tell()
    lines=(tail+data).split(b'\n');tail=lines.pop()
    for line in lines:
        fields=line.decode('utf8').split('\t')
        if len(fields)==5:records.append([int(fields[0]),int(fields[1]),int(fields[2]),fields[3],fields[4]])
def measure():
    m=Counters();m.cb=c.sizeof(m);assert ps.GetProcessMemoryInfo(handle,c.byref(m),m.cb)
    memory.append({'t':time.perf_counter(),'private':m.private,'gdi':u.GetGuiResources(handle,0),'user':u.GetGuiResources(handle,1)})
def await_paint(name,start):
    deadline=time.monotonic()+10
    while time.monotonic()<deadline:
        read_trace();measure()
        for row in records[-1000:]:
            if row[0]==15 and row[2]>=start and Path(row[4]).name==name:return row[2]
        if process.poll() is not None:raise AssertionError('viewer exited')
        time.sleep(.001)
    raise AssertionError('no matching rendered frame: '+name)
def verify_pixel(color):
    rect=w.RECT();assert u.GetClientRect(hwnd,c.byref(rect))
    dc=u.GetDC(hwnd);mem=g.CreateCompatibleDC(dc);bitmap=g.CreateCompatibleBitmap(dc,rect.right,rect.bottom);old=g.SelectObject(mem,bitmap)
    try:
        assert u.PrintWindow(hwnd,mem,3)
        value=g.GetPixel(mem,rect.right//2,rect.bottom//2)
        actual=(value&255,(value>>8)&255,(value>>16)&255)
        assert max(abs(a-b) for a,b in zip(actual,color))<=8,(actual,color)
        image_index=colors.index(color)
        for corner,(px,py) in enumerate([(0.2,0.2),(0.8,0.2),(0.2,0.8),(0.8,0.8)]):
            value=g.GetPixel(mem,int(rect.right*px),int(rect.bottom*py))
            actual=(value&255,(value>>8)&255,(value>>16)&255)
            expected=corner_colors[corner_map[image_index][corner]]
            assert max(abs(a-b) for a,b in zip(actual,expected))<=8,('EXIF corner',image_index+1,corner,actual,expected)
    finally:g.SelectObject(mem,old);g.DeleteObject(bitmap);g.DeleteDC(mem);u.ReleaseDC(hwnd,dc)
def percentile(values,p):return sorted(values)[max(0,math.ceil(len(values)*p)-1)]
process=subprocess.Popen([str(viewer.resolve()),'/window',str((scratch/'00.jpg').resolve())],cwd=scratch,env=env)
handle=k.OpenProcess(0x410,False,process.pid);assert handle
hwnd=None;failure=None
try:
    wait(lambda:any(name=='fyto' for _,name in wins(process.pid)))
    hwnd=next(h for h,name in wins(process.pid) if name=='fyto')
    await_paint('00.jpg',0);verify_pixel(colors[0]);time.sleep(.5)
    index=0;direction=1
    for i in range(1100):
        if index==7:direction=-1
        elif index==0:direction=1
        index+=direction;start=int(time.perf_counter()*freq)
        assert u.PostMessageW(hwnd,0x20A,((-120 if direction==1 else 120)&65535)<<16,0)
        end=await_paint(f'{index:02}.jpg',start)
        verify_pixel(colors[index])
        samples.append({'iteration':i,'index':index,'input_qpc':start,'paint_qpc':end,'ms':(end-start)*1000/freq})
        if i<100:time.sleep(.22)
        if i in [99,599,1099]:
            # Compare the same file in the same settled state, rather than a
            # decoding midpoint against an idle endpoint.
            while index>0:
                index-=1;t=int(time.perf_counter()*freq)
                assert u.PostMessageW(hwnd,0x20A,120<<16,0)
                await_paint(f'{index:02}.jpg',t);verify_pixel(colors[index])
            direction=1;time.sleep(1);read_trace();measure()
            checkpoints.append(dict(memory[-1],iteration=i))
    time.sleep(1);read_trace();measure()
except BaseException as exc:failure=repr(exc)
finally:
    if hwnd:u.PostMessageW(hwnd,0x10,0,0)
    try:process.wait(timeout=5)
    except subprocess.TimeoutExpired:process.kill();process.wait(timeout=5)
    k.CloseHandle(handle)
warm=[x['ms'] for x in samples[100:]]
ui_start=samples[100]['input_qpc'] if len(samples)>100 else 0
ui=[(row[2]-row[1])*1000/freq for row in records if row[1]>=ui_start and row[0] in [15,0x20A,0x100]]
checks={'complete_1000':len(warm)==1000,'warm_p95_le_33ms':bool(warm) and percentile(warm,.95)<=33,
    'ui_p95_le_8ms':bool(ui) and percentile(ui,.95)<=8,'ui_max_le_16ms':bool(ui) and max(ui)<=16,
    'private_peak_le_256MiB':bool(memory) and max(x['private'] for x in memory)<=256*1024**2,
    'gdi_stable':len(checkpoints)==3 and checkpoints[-1]['gdi']<=checkpoints[0]['gdi']+2,
    'no_private_growth':len(checkpoints)==3 and checkpoints[-1]['private']<=checkpoints[0]['private']+4*1024**2}
result={'platform':platform.platform(),'exe_sha256':hashlib.sha256(viewer.read_bytes()).hexdigest(),'commit':os.getenv('GITHUB_SHA'),
    'scope':'CI host; input posting to completed application WM_PAINT; excludes physical scanout; generated 24 MP local JPEGs with EXIF 1-8 corner verification, no NAS',
    'checkpoints':checkpoints,'display_dimensions':[u.GetSystemMetrics(0),u.GetSystemMetrics(1)],'fixtures':fixtures,'failure':failure,'checks':checks,'warm_p95_ms':percentile(warm,.95) if warm else None,
    'ui_p95_ms':percentile(ui,.95) if ui else None,'ui_max_ms':max(ui) if ui else None,
    'private_peak_MiB':max((x['private'] for x in memory),default=0)/1024**2,'samples':samples,'resources':memory}
(out/'results.json').write_text(json.dumps(result,indent=2),encoding='utf8')
print(json.dumps({k:v for k,v in result.items() if k not in ['samples','resources','fixtures']},indent=2),flush=True)
if failure or not all(checks.values()):raise SystemExit(1)
