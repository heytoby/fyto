from window_test_helpers import *
import csv,io
u.SetProcessDPIAware()
u.GetWindowLongW.argtypes=[w.HWND,c.c_int];u.GetWindowLongW.restype=w.LONG
root=Path('work/fyto-v27-update-check').resolve();root.mkdir(exist_ok=True)
shutil.copy2('work/fyto-v27-source/voidImageViewer/vs2019/x64/Release/fyto.exe',root/'fyto.exe')
(root/'fyto.ini').write_text('[fyto]\nappdata=0\nlanguage=1\nmultiple_instances=1\ndefaults_version=2\nshow_caption=1\nshow_menu=1\nshow_status=1\nshow_controls=1\nmaximized=0\n')
(root/'associations.ini').write_text('[fyto]\nSelectedFormats=15\nSetupVersion=1\n')
(root/'updates.ini').write_text('[updates]\nrepository=invalid\n')
k=c.WinDLL('kernel32',use_last_error=True)
class PE(c.Structure):
    _fields_=[('dwSize',w.DWORD),('cntUsage',w.DWORD),('th32ProcessID',w.DWORD),('th32DefaultHeapID',c.c_size_t),('th32ModuleID',w.DWORD),('cntThreads',w.DWORD),('th32ParentProcessID',w.DWORD),('pcPriClassBase',w.LONG),('dwFlags',w.DWORD),('szExeFile',w.WCHAR*260)]
k.CreateToolhelp32Snapshot.argtypes=[w.DWORD,w.DWORD];k.CreateToolhelp32Snapshot.restype=w.HANDLE
k.Process32FirstW.argtypes=[w.HANDLE,c.POINTER(PE)];k.Process32NextW.argtypes=[w.HANDLE,c.POINTER(PE)];k.CloseHandle.argtypes=[w.HANDLE]
def updater_pids():
    snapshot=k.CreateToolhelp32Snapshot(2,0);entry=PE();entry.dwSize=c.sizeof(entry);found=set()
    valid=k.Process32FirstW(snapshot,c.byref(entry))
    while valid:
        if entry.szExeFile.lower()=='fyto-updater.exe':found.add(entry.th32ProcessID)
        valid=k.Process32NextW(snapshot,c.byref(entry))
    k.CloseHandle(snapshot);return found
before=updater_pids();process=None;helper=None
try:
    process=subprocess.Popen([str(root/'fyto.exe'),'/setup','/window'],cwd=root)
    wait(lambda:any(cls=='fyto' for _,cls in wins(process.pid)))
    h=next(h for h,cls in wins(process.pid) if cls=='fyto');time.sleep(.5)
    blank=rect(h);assert blank!=(0,0,2560,1440),blank
    assert not updater_pids()-before,'startup unexpectedly launched updater'
    u.SendMessageW(h,0x111,60004,0)
    wait(lambda:bool(updater_pids()-before));helper=next(iter(updater_pids()-before))
    wait(lambda:any('检查更新' in label(window) for window,_ in wins(helper)))
    updater=next(window for window,_ in wins(helper) if '检查更新' in label(window))
    wait(lambda:any('有效的 GitHub' in label(window) for window,_ in wins(helper,updater)))
    assert process.poll() is None,'failed update closed viewer'
    u.PostMessageW(updater,0x10,0,0)
    result=subprocess.run([str(Path('work/fyto-installer-v27/fyto-Setup.exe').resolve()),'/close-running',str(process.pid)],timeout=15)
    assert result.returncode==0,result.returncode
    assert process.wait(timeout=5)==0
    report={'blank_setup_window':blank,'startup_update_check':'absent','manual_update_menu':'opens embedded updater','invalid_repository':'clear error; viewer remains open','installer_closes_running_fyto':'graceful closure verified with scoped test PID','online_upgrade_installation':'not executed: published repository/release required'}
    Path('work/fyto-v27-update-validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(report))
finally:
    if process and process.poll() is None:process.terminate();process.wait(timeout=5)
    if helper:
        for window,_ in wins(helper):u.PostMessageW(window,0x10,0,0)
