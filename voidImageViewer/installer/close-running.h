#include <tlhelp32.h>
static BOOL CALLBACK close_window(HWND hwnd,LPARAM pid)
{
    DWORD owner;GetWindowThreadProcessId(hwnd,&owner);
    if(owner==(DWORD)pid)PostMessageW(hwnd,WM_CLOSE,0,0);
    return TRUE;
}
// Restrict termination to fyto.exe in this Windows session; helpers survive.
static BOOL close_running_fyto(DWORD only_pid)
{
    HANDLE snapshot,handles[256];DWORD count=0,session,current_session,start,i;
    PROCESSENTRY32W entry;BOOL ok=TRUE;
    if(!ProcessIdToSessionId(GetCurrentProcessId(),&current_session))return FALSE;
    snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(snapshot==INVALID_HANDLE_VALUE)return FALSE;
    ZeroMemory(&entry,sizeof(entry));entry.dwSize=sizeof(entry);
    if(Process32FirstW(snapshot,&entry))do {
        if(_wcsicmp(entry.szExeFile,L"fyto.exe") || entry.th32ProcessID==GetCurrentProcessId() || (only_pid && entry.th32ProcessID!=only_pid))continue;
        if(!ProcessIdToSessionId(entry.th32ProcessID,&session) || session!=current_session)continue;
        if(count==256){ok=FALSE;break;}
        handles[count]=OpenProcess(SYNCHRONIZE|PROCESS_TERMINATE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,entry.th32ProcessID);
        if(!handles[count]){if(GetLastError()!=ERROR_INVALID_PARAMETER)ok=FALSE;continue;}
        EnumWindows(close_window,(LPARAM)entry.th32ProcessID);count++;
    }while(Process32NextW(snapshot,&entry));
    CloseHandle(snapshot);start=GetTickCount();
    for(;;){BOOL alive=FALSE;for(i=0;i<count;i++)if(WaitForSingleObject(handles[i],0)==WAIT_TIMEOUT)alive=TRUE;
        if(!alive || GetTickCount()-start>=8000)break;Sleep(50);
    }
    for(i=0;i<count;i++){
        if(WaitForSingleObject(handles[i],0)==WAIT_TIMEOUT){
            if(!TerminateProcess(handles[i],0) || WaitForSingleObject(handles[i],3000)!=WAIT_OBJECT_0)ok=FALSE;
        }
        CloseHandle(handles[i]);
    }
    return ok;
}
