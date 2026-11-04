from window_test_helpers import *
import struct
u.SetProcessDPIAware()
u.IsZoomed.argtypes=[w.HWND];u.IsZoomed.restype=w.BOOL
u.ShowWindow.argtypes=[w.HWND,c.c_int]
u.GetWindowLongW.argtypes=[w.HWND,c.c_int];u.GetWindowLongW.restype=w.LONG
class MI(c.Structure):_fields_=[('size',w.DWORD),('monitor',w.RECT),('work',w.RECT),('flags',w.DWORD)]
u.GetMonitorInfoW.argtypes=[w.HANDLE,c.POINTER(MI)]
MCB=c.WINFUNCTYPE(w.BOOL,w.HANDLE,w.HDC,c.POINTER(w.RECT),w.LPARAM)
monitors=[]
@MCB
def enum(h,dc,r,param):
    info=MI();info.size=c.sizeof(info);assert u.GetMonitorInfoW(h,c.byref(info));monitors.append(tuple(getattr(info.work,x) for x in ['left','top','right','bottom']));return True
u.EnumDisplayMonitors(None,None,enum,0)
class CD(c.Structure):_fields_=[('data',c.c_size_t),('size',w.DWORD),('pointer',c.c_void_p)]
class GUI(c.Structure):
    _fields_=[('size',w.DWORD),('flags',w.DWORD),('active',w.HWND),('focus',w.HWND),('capture',w.HWND),('menu',w.HWND),('move',w.HWND),('caret',w.HWND),('caret_rect',w.RECT)]
u.GetGUIThreadInfo.argtypes=[w.DWORD,c.POINTER(GUI)]
def command(h,text):
    raw=struct.pack('<I',1)+(text+'\0'+str(root)+'\0').encode('utf-16le');buf=c.create_string_buffer(raw);cd=CD(0,len(raw),c.cast(buf,c.c_void_p))
    assert u.SendMessageW(h,0x4a,0,c.addressof(cd))==1
def lp(x,y):return (x&65535)|((y&65535)<<16)
def full(h):return not (u.GetWindowLongW(h,-16)&0xc00000)
project=Path(__file__).resolve().parents[1]
root=project/'build/window-check';root.mkdir(parents=True,exist_ok=True)
shutil.copy2(project/'voidImageViewer/vs2019/x64/Release/fyto.exe',root/'fyto.exe')
(root/'fyto.ini').write_text('[fyto]\nappdata=0\nlanguage=1\nmultiple_instances=1\ndefaults_version=2\nshow_caption=1\nshow_menu=1\nshow_status=1\nshow_controls=1\nmaximized=1\n')
(root/'associations.ini').write_text('[fyto]\nSelectedFormats=15\nSetupVersion=1\n')
Image.new('RGB',(2400,1200),(80,110,150)).save(root/'photo.png')
p=subprocess.Popen([str(root/'fyto.exe'),'/setup','/window'],cwd=root)
report={'version':'1.0.0.28','empty_views':[],'fullscreen_drags':[]}
try:
    wait(lambda:any(cls=='fyto' for _,cls in wins(p.pid)));h=next(h for h,cls in wins(p.pid) if cls=='fyto');time.sleep(.3)
    assert not u.IsZoomed(h) and not full(h)
    for work in monitors:
        l,t,r,b=work;u.SetWindowPos(h,0,l+15,t+15,400,500,0x14)
        u.ShowWindow(h,3);time.sleep(.1)
        command(h,'fyto /setup /window');time.sleep(.15)
        window=rect(h);width=window[2]-window[0];height=window[3]-window[1]
        assert not u.IsZoomed(h) and not full(h)
        assert width>height and abs(width*5-height*8)<=8,(work,window)
        assert window[0]>=l and window[1]>=t and window[2]<=r and window[3]<=b,(work,window)
        report['empty_views'].append({'work':work,'window':window})
    command(h,'fyto "'+str(root/'photo.png')+'"');wait(lambda:full(h));time.sleep(.4)
    for minimal in [False,True]:
        u.SendMessageW(h,0x111,1016 if minimal else 1018,0);time.sleep(.2)
        before=rect(h);x=(before[0]+before[2])//2;y=before[1]+14
        assert u.SendMessageW(h,0x84,0,lp(x,y))==2,(minimal,'no caption drag hit')
        # A click and movements below the system threshold must not leave fullscreen.
        u.SendMessageW(h,0xa1,2,lp(x,y))
        u.PostMessageW(h,0x202,0,0);time.sleep(.1);assert full(h) and rect(h)==before
        u.SendMessageW(h,0xa1,2,lp(x,y))
        point=w.POINT(x+50,y+70);u.ScreenToClient(h,c.byref(point))
        u.PostMessageW(h,0x200,1,lp(point.x,point.y))
        time.sleep(.2);u.PostMessageW(h,0x1f,0,0)
        wait(lambda:not full(h));time.sleep(.2)
        restored=rect(h);assert not u.IsZoomed(h)
        assert restored[2]-restored[0]>=400 and restored[3]-restored[1]>=200,restored
        report['fullscreen_drags'].append({'minimal':minimal,'from':before,'restored':restored})
        u.PostMessageW(h,0x111,1024,0);wait(lambda:full(h));time.sleep(.2)
    u.PostMessageW(h,0x111,1024,0);wait(lambda:not full(h));time.sleep(.2)
    report['split_and_maximized_drags']=[]
    for minimal in [True,False]:
        u.SendMessageW(h,0x111,1016 if minimal else 1018,0);time.sleep(.2)
        for maximized in [False,True]:
            l,t,r,b=monitors[0];u.SetWindowPos(h,0,l,t,(r-l)//2,b-t,0x14)
            if maximized:u.ShowWindow(h,3)
            time.sleep(.2);before=rect(h);x=(before[0]+before[2])//2;y=before[1]+14
            points=[px for px in range(before[0]+90,before[2]-135,8) if u.SendMessageW(h,0x84,0,lp(px,y))==2]
            assert points,(minimal,maximized,before,'no draggable caption area')
            x=min(points,key=lambda px:abs(px-x))
            u.SendMessageW(h,0xa1,2,lp(x,y));u.PostMessageW(h,0x202,0,0);time.sleep(.1)
            assert rect(h)==before,'click changed window'
            u.SendMessageW(h,0xa1,2,lp(x,y))
            point=w.POINT(x+60,y+90);u.ScreenToClient(h,c.byref(point))
            u.PostMessageW(h,0x200,1,lp(point.x,point.y));time.sleep(.15);u.PostMessageW(h,0x1f,0,0)
            wait(lambda:rect(h)!=before and not u.IsZoomed(h));time.sleep(.15)
            after=rect(h);assert after[2]-after[0]>=400 and after[3]-after[1]>=200
            report['split_and_maximized_drags'].append({'minimal':minimal,'maximized':maximized,'from':before,'restored':after})

finally:
    for window,cls in wins(p.pid):
        if cls=='fyto':u.PostMessageW(window,0x1f,0,0);u.PostMessageW(window,0x10,0,0)
    try:p.wait(timeout=5)
    except subprocess.TimeoutExpired:p.terminate();p.wait(timeout=5)
(root/'validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(report))
