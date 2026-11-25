"""Windows native lifetime commit peak; periodic private-memory samples are separate."""
import ctypes as c,ctypes.wintypes as w
class Counters(c.Structure):
    _fields_=[('cb',w.DWORD),('faults',w.DWORD)]+[(n,c.c_size_t) for n in
        ['peak_ws','ws','peak_paged','paged','peak_nonpaged','nonpaged','pagefile','peak_pagefile','private']]
k=c.WinDLL('kernel32',use_last_error=True);ps=c.WinDLL('psapi',use_last_error=True)
k.OpenProcess.argtypes=[w.DWORD,w.BOOL,w.DWORD];k.OpenProcess.restype=w.HANDLE
k.CloseHandle.argtypes=[w.HANDLE]
ps.GetProcessMemoryInfo.argtypes=[w.HANDLE,c.POINTER(Counters),w.DWORD]
def process_resources(pid):
    handle=k.OpenProcess(0x410,False,pid)
    if not handle:raise c.WinError(c.get_last_error())
    try:
        value=Counters();value.cb=c.sizeof(value)
        if not ps.GetProcessMemoryInfo(handle,c.byref(value),value.cb):raise c.WinError(c.get_last_error())
        return {'private_MiB':value.private/1024**2,'private_commit_peak_MiB':value.peak_pagefile/1024**2,
                'working_set_MiB':value.ws/1024**2,'working_set_peak_MiB':value.peak_ws/1024**2}
    finally:k.CloseHandle(handle)
