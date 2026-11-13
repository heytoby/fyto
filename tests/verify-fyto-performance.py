from pathlib import Path
from PIL import Image, ImageDraw
from statistics import median
from tempfile import TemporaryDirectory
import ctypes as c
from ctypes import wintypes as w
import json, os, random, shutil, struct, subprocess, sys, time

root=Path(sys.argv[1])
baseline=Path(sys.argv[2])
candidate=Path(sys.argv[3])
installed=Path(sys.argv[4])
root.mkdir(exist_ok=True,parents=True)
os.chdir(root)

u=c.WinDLL('user32',use_last_error=True)
k=c.WinDLL('kernel32',use_last_error=True)
papi=c.WinDLL('psapi')
CB=c.WINFUNCTYPE(w.BOOL,w.HWND,w.LPARAM)
u.EnumWindows.argtypes=[CB,w.LPARAM]
u.GetWindowThreadProcessId.argtypes=[w.HWND,c.POINTER(w.DWORD)]
u.GetClassNameW.argtypes=[w.HWND,w.LPWSTR,c.c_int]
u.GetWindowTextW.argtypes=[w.HWND,w.LPWSTR,c.c_int]
u.PostMessageW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM]
u.GetGuiResources.argtypes=[w.HANDLE,w.DWORD]
u.SendMessageTimeoutW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM,w.UINT,w.UINT,c.POINTER(c.c_size_t)]
u.SendMessageTimeoutW.restype=c.c_ssize_t
k.QueryPerformanceCounter.argtypes=[c.POINTER(c.c_longlong)]
k.QueryPerformanceFrequency.argtypes=[c.POINTER(c.c_longlong)]
freq=c.c_longlong()
k.QueryPerformanceFrequency(c.byref(freq))

class Memory(c.Structure):
    _fields_=[('cb',w.DWORD),('faults',w.DWORD),('peak_ws',c.c_size_t),('ws',c.c_size_t),
              ('quota_peak_paged',c.c_size_t),('quota_paged',c.c_size_t),
              ('quota_peak_nonpaged',c.c_size_t),('quota_nonpaged',c.c_size_t),
              ('pagefile',c.c_size_t),('peak_pagefile',c.c_size_t),('private',c.c_size_t)]
papi.GetProcessMemoryInfo.argtypes=[w.HANDLE,c.POINTER(Memory),w.DWORD]

def memory(process):
    info=Memory()
    info.cb=c.sizeof(info)
    assert papi.GetProcessMemoryInfo(w.HANDLE(process._handle),c.byref(info),info.cb)
    return {name:round(getattr(info,name)/2**20,1) for name in ('peak_ws','ws','private','peak_pagefile')}

def qpc():
    value=c.c_longlong()
    k.QueryPerformanceCounter(c.byref(value))
    return value.value

def ms(start,end):
    return round((end-start)*1000/freq.value,2)

def find_window(pid):
    matches=[]
    @CB
    def visit(hwnd,parameter):
        found=w.DWORD()
        u.GetWindowThreadProcessId(hwnd,c.byref(found))
        if found.value==pid:
            cls=c.create_unicode_buffer(32)
            u.GetClassNameW(hwnd,cls,len(cls))
            if cls.value=='fyto':matches.append(hwnd)
        return True
    u.EnumWindows(visit,0)
    return matches[0] if matches else None

def title(hwnd):
    name=c.create_unicode_buffer(1024)
    u.GetWindowTextW(hwnd,name,len(name))
    return name.value

def records(path):
    try: data=path.read_bytes()
    except (FileNotFoundError,PermissionError):return []
    return list(struct.iter_unpack('<qII',data[:len(data)//16*16]))

def await_value(proc,predicate,trace,limit=10):
    until=time.monotonic()+limit
    while time.monotonic()<until:
        value=predicate()
        if value:return value
        if proc.poll() is not None:break
        time.sleep(.004)
    raise AssertionError((proc.poll(),records(trace)[-20:]))

def event(path,kinds,index=None,after=0):
    return next((ts for ts,kind,n in records(path) if ts>=after and kind in kinds and
                 (index is None or index==n)),None)

def scroll(hwnd,down=True):
    ts=qpc()
    assert u.PostMessageW(hwnd,0x020a,((-120 if down else 120)&65535)<<16,0)
    return ts

def make_fixtures():
    random_bytes=random.Random(1229).randbytes(6000*4000*3)
    image=Image.frombytes('RGB',(6000,4000),random_bytes)
    for kind in ('jpg','png'):
        folder=root/kind
        folder.mkdir(exist_ok=True)
        for i in range(1,13):
            if kind=='png' and i>6:break
            color=(i*29%256,i*53%256,i*73%256)
            ImageDraw.Draw(image).rectangle((1800,1200,4200,2800),fill=color)
            path=folder/f'{i:03}.{kind}'
            image.save(path,quality=91,compress_level=1)
            os.utime(path,ns=(1700000000000000000,1700000000000000000))
    folder=root/'order'
    folder.mkdir(exist_ok=True)
    for i in range(1,9):
        file=folder/f'{i:03}.png'
        Image.new('RGB',(800,600),(i*29%256,i*53%256,i*73%256)).save(file)
        os.utime(file,ns=(1700000000000000000,1700000000000000000))

def launch(exe,first,session,preload,**extra):
    folder=root/session
    folder.mkdir(exist_ok=True)
    target=folder/'fyto.exe'
    shutil.copyfile(exe,target)
    (folder/'fyto.ini').write_text('[fyto]\nappdata=0\nmultiple_instances=1\ndefaults_version=5\n'
        'show_caption=0\nshow_menu=0\nmaximized=0\npreload_next='+str(int(preload))+'\ncache_last=1\n'
        'sort=2\nsort_ascending=1\nmouse_wheel_action=1\norientation=1\nicm=1\nshow_exif=0\n',encoding='ascii')
    (folder/'associations.ini').write_text('[fyto]\nSelectedFormats=15\nSetupVersion=1\n',encoding='ascii')
    (folder/'first-run.complete').touch()
    trace=folder/'trace.bin'
    if trace.exists():trace.unlink()
    startup=subprocess.STARTUPINFO()
    startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow=0
    start=qpc()
    proc=subprocess.Popen([str(target),'/window',str(first)],cwd=folder,
        env=dict(os.environ,FYTO_TEST_TRACE=str(trace),**extra),
        startupinfo=startup,creationflags=subprocess.CREATE_NO_WINDOW)
    return proc,trace,start

def close(proc,hwnd):
    if hwnd:u.PostMessageW(hwnd,0x10,0,0)
    try:proc.wait(timeout=4)
    except subprocess.TimeoutExpired:proc.kill();proc.wait(timeout=4)
    assert proc.returncode==0,proc.returncode

def profile(exe,label,kind,delay,preload=True,release=False):
    count=12 if kind=='jpg' else 6
    proc,trace,start=launch(exe,root/kind/f'001.{kind}',f'{label}-{kind}-{delay}-{preload}',preload)
    hwnd=None
    try:
        hwnd=await_value(proc,lambda:find_window(proc.pid),trace)
        def painted(i,since=0,full=False):
            if release:
                return await_value(proc,lambda:qpc() if title(hwnd)==f'{i:03}.{kind}' else None,trace)
            return await_value(proc,lambda:event(trace,(2,) if full else (1,2),i,since),trace)
        first=painted(1)
        time.sleep(.85)
        times=[]
        for i in range(2,count+1):
            time.sleep(delay)
            t=scroll(hwnd)
            times.append(ms(t,painted(i,t)))
        full=ms(t,painted(count,t,full=True)) if not release else None
        time.sleep(.20)
        stats=memory(proc)
        result=dict(label=label,kind=kind,interval=delay,preload=preload,opening=ms(start,first),
            median=median(times),maximum=max(times),times=times,last_full=full,memory=stats,
            first_events=[(ms(start,ts),k,n) for ts,k,n in records(trace)[:180]])
        print('FYTO_PERF '+json.dumps(result),flush=True)
        return result
    finally:
        close(proc,hwnd)

def wheel_regression():
    for name in ('burst','reverse','directory'):
        extra={'FYTO_TEST_SLOW_DIRECTORY' if name=='directory' else 'FYTO_TEST_SLOW_IMAGE':'1'}
        proc,trace,_=launch(candidate,root/'order/003.png',name,name=='directory',**extra)
        hwnd=None
        try:
            hwnd=await_value(proc,lambda:find_window(proc.pid),trace)
            await_value(proc,lambda:event(trace,(2,),3),trace)
            if name=='directory':
                await_value(proc,lambda:event(trace,(90,)),trace)
                value=c.c_size_t()
                assert u.SendMessageTimeoutW(hwnd,0,0,0,2,250,c.byref(value))
            t=scroll(hwnd)
            if name!='directory':await_value(proc,lambda:event(trace,(91,),after=t),trace)
            if name=='reverse':
                t=scroll(hwnd,False)
                expected=[2]
            else:
                scroll(hwnd)
                scroll(hwnd)
                expected=[4,5,6]
            await_value(proc,lambda:event(trace,(1,2),expected[-1],t),trace)
            time.sleep(.75)
            seen=[]
            for ts,k,i in records(trace):
                if ts>=t and k in (1,2) and i and (not seen or seen[-1]!=i):seen.append(i)
            assert seen==expected,(name,seen,expected)
            print('FYTO_WHEEL',name,seen,flush=True)
        finally:
            close(proc,hwnd)

make_fixtures()
profile(candidate,'diagnostic','jpg',.24)
