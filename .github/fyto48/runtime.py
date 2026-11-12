from pathlib import Path
from statistics import median
from PIL import Image
import ctypes as c
from ctypes import wintypes as w
import json, os, shutil, struct, subprocess, sys, threading, time

root, old, new = map(Path, sys.argv[1:4])
case=root/'stress'
case.mkdir()
noise=Image.effect_noise((6000,4000),55).convert('RGB')
noise.save(root/'test.jpg',quality=92)
noise.save(root/'test.png',compress_level=2)
for index in range(1,19):
    suffix = 'jpg' if index % 3 else 'png'
    dst = case/f'{index:03}-image.{suffix}'
    try: os.link(root/f'test.{suffix}',dst)
    except OSError: shutil.copyfile(root/f'test.{suffix}',dst)
    os.utime(dst,ns=(1700000000000000000,1700000000000000000))
del noise

u=c.WinDLL('user32')
k=c.WinDLL('kernel32')
papi=c.WinDLL('psapi')
CB=c.WINFUNCTYPE(w.BOOL,w.HWND,w.LPARAM)
u.EnumWindows.argtypes=[CB,w.LPARAM]
u.GetWindowThreadProcessId.argtypes=[w.HWND,c.POINTER(w.DWORD)]
u.GetClassNameW.argtypes=[w.HWND,w.LPWSTR,c.c_int]
u.PostMessageW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM]
u.SendMessageTimeoutW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM,w.UINT,w.UINT,c.POINTER(c.c_size_t)]
u.SendMessageTimeoutW.restype=c.c_ssize_t
u.GetGuiResources.argtypes=[w.HANDLE,w.DWORD]
k.QueryPerformanceFrequency.argtypes=[c.POINTER(c.c_longlong)]
k.QueryPerformanceCounter.argtypes=[c.POINTER(c.c_longlong)]
hz=c.c_longlong()
k.QueryPerformanceFrequency(c.byref(hz))

class PMC(c.Structure):
    _fields_=[('cb',w.DWORD),('faults',w.DWORD),('peak_ws',c.c_size_t),('ws',c.c_size_t),
       ('quota_peak_paged',c.c_size_t),('quota_paged',c.c_size_t),('quota_peak_nonpaged',c.c_size_t),
       ('quota_nonpaged',c.c_size_t),('pagefile',c.c_size_t),('peak_pagefile',c.c_size_t),('private',c.c_size_t)]
papi.GetProcessMemoryInfo.argtypes=[w.HANDLE,c.POINTER(PMC),w.DWORD]

def now():
    t=c.c_longlong()
    k.QueryPerformanceCounter(c.byref(t))
    return t.value

def ms(t0,t1): return round(1000*(t1-t0)/hz.value,2)

def window(pid):
    handles=[]
    @CB
    def visit(hwnd,param):
        p=w.DWORD()
        u.GetWindowThreadProcessId(hwnd,c.byref(p))
        if p.value==pid:
            name=c.create_unicode_buffer(64)
            u.GetClassNameW(hwnd,name,len(name))
            if name.value=='fyto': handles.append(hwnd)
        return True
    u.EnumWindows(visit,0)
    return handles[0] if handles else None

def records(path):
    try: data=path.read_bytes()
    except (FileNotFoundError,PermissionError): return []
    return list(struct.iter_unpack('<qII',data[:len(data)//16*16]))

def event(trace,kinds,value=None,since=0):
    return next((t for t,event,n in records(trace) if t>=since and event in kinds and
                 (value is None or n==value)),None)

def wait(proc,f,trace,limit=12):
    end=time.monotonic()+limit
    while time.monotonic()<end:
        value=f()
        if value: return value
        if proc.poll() is not None: break
        time.sleep(.004)
    raise AssertionError((proc.poll(), records(trace)[-24:]))

def stats(proc):
    s=PMC()
    s.cb=c.sizeof(s)
    assert papi.GetProcessMemoryInfo(w.HANDLE(proc._handle),c.byref(s),s.cb)
    return {'working':round(s.ws/2**20,1),'private':round(s.private/2**20,1),
            'peak':round(s.peak_ws/2**20,1),'gdi':u.GetGuiResources(w.HANDLE(proc._handle),0)}

def launch(exe,name,**extra):
    folder=root/name
    folder.mkdir()
    target=folder/'fyto.exe'
    shutil.copyfile(exe,target)
    (folder/'fyto.ini').write_text('[fyto]\nappdata=0\nmultiple_instances=1\ndefaults_version=5\n'
        'show_caption=1\nshow_menu=1\nmaximized=0\npreload_next=1\ncache_last=1\n'
        'sort=2\nsort_ascending=1\nmouse_wheel_action=1\norientation=1\nicm=1\nshow_exif=0\n',encoding='ascii')
    (folder/'associations.ini').write_text('[fyto]\nSelectedFormats=15\nSetupVersion=1\n',encoding='ascii')
    (folder/'first-run.complete').touch()
    trace=folder/'trace.bin'
    startup=subprocess.STARTUPINFO()
    startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow=0
    proc=subprocess.Popen([str(target),'/window',str(case/'001-image.jpg')],
        cwd=folder,env=dict(os.environ,FYTO_TEST_TRACE=str(trace),**extra),
        startupinfo=startup,creationflags=subprocess.CREATE_NO_WINDOW)
    return proc,trace

def close(proc,hwnd,timeout=4):
    if hwnd: u.PostMessageW(hwnd,0x10,0,0)
    try: proc.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait(timeout=3)
        return False
    return proc.returncode==0

def ping(hwnd,timeout=250):
    value=c.c_size_t()
    t=now()
    result=u.SendMessageTimeoutW(hwnd,0,0,0,2,timeout,c.byref(value))
    return ms(t,now()),bool(result)

def scroll(hwnd,next_=True):
    t=now()
    assert u.PostMessageW(hwnd,0x020a,((-120 if next_ else 120)&65535)<<16,0)
    return t

def browsing(exe,name):
    proc,trace=launch(exe,name+'-browse')
    hwnd=None
    stop=threading.Event()
    observer=None
    latencies=[]
    try:
        hwnd=wait(proc,lambda:window(proc.pid),trace)
        wait(proc,lambda:event(trace,(2,),1),trace)
        def observe():
            while not stop.is_set():
                latencies.append(ping(hwnd))
                stop.wait(.025)
        observer=threading.Thread(target=observe,daemon=True)
        observer.start()
        time.sleep(.90)
        before=stats(proc)
        delays=[]
        for offset,number in enumerate(list(range(2,19))+list(range(17,0,-1))):
            time.sleep(.12)
            t=scroll(hwnd,offset<17)
            shown=wait(proc,lambda:event(trace,(1,2),number,t),trace)
            delays.append(ms(t,shown))
            if number in (5,10,18,1): time.sleep(.70)
        time.sleep(1.10)
        after=stats(proc)
        return {'before':before,'after':after,'first8_ms':round(median(delays[:8]),2),
            'last8_ms':round(median(delays[-8:]),2),'median_ms':round(median(delays),2),
            'slowest_window_ms':max(t for t,_ in latencies),'timeouts':sum(not ok for _,ok in latencies)}
    finally:
        stop.set()
        if observer: observer.join(timeout=.5)
        assert close(proc,hwnd)

def delayed_directory(exe,name):
    proc,trace=launch(exe,name+'-directory',FYTO_TEST_SLOW_DIRECTORY='1')
    hwnd=None
    try:
        hwnd=wait(proc,lambda:window(proc.pid),trace)
        wait(proc,lambda:event(trace,(90,)),trace)
        elapsed,ok=ping(hwnd)
        if name=='new':
            assert ok,(name,elapsed)
            tick=scroll(hwnd)
            scroll(hwnd)
            scroll(hwnd)
            wait(proc,lambda:event(trace,(1,2),4,tick),trace)
        else:
            assert not ok,(name,elapsed)
        return {'ms':elapsed,'responded':ok}
    finally:
        assert close(proc,hwnd)

def stalled_close(exe,name):
    proc,trace=launch(exe,name+'-close',FYTO_TEST_SLOW_IMAGE='1')
    hwnd=None
    try:
        hwnd=wait(proc,lambda:window(proc.pid),trace)
        wait(proc,lambda:event(trace,(91,)),trace)
        elapsed=now()
        exited=close(proc,hwnd,timeout=1.4)
        hwnd=None
        return {'ms':ms(elapsed,now()),'closed':exited}
    finally:
        if hwnd: close(proc,hwnd)

out={}
for name,exe in [('old',old),('new',new)]:
    out[name]={'browse':browsing(exe,name),'directory':delayed_directory(exe,name),'close':stalled_close(exe,name)}
    print('FYTO_RESULT '+name+' '+json.dumps(out[name],separators=(',',':')),flush=True)
assert out['new']['close']['closed'] and not out['old']['close']['closed'],out
assert out['new']['browse']['timeouts']==0,out
assert out['new']['browse']['after']['private'] <= .7*out['old']['browse']['after']['private'],out
