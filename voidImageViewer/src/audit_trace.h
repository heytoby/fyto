#ifndef FYTO_AUDIT_TRACE_H
#define FYTO_AUDIT_TRACE_H
/* Explicit opt-in local test trace. No trace file or timing calls when unset. */
static HANDLE fyto_audit_file=INVALID_HANDLE_VALUE;
static int fyto_audit_initialized;
static BOOL fyto_audit_enabled(void){
    if(!fyto_audit_initialized){
        wchar_t path[32768];fyto_audit_initialized=1;
        if(GetEnvironmentVariableW(L"FYTO_AUDIT_TRACE",path,ARRAYSIZE(path))>0)
            fyto_audit_file=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    }
    return fyto_audit_file!=INVALID_HANDLE_VALUE;
}
static void fyto_audit_record(UINT message,LARGE_INTEGER begin,LARGE_INTEGER end,
    const wchar_t *requested,const wchar_t *shown){
    char line[256],a[4096],b[4096];DWORD written;int count;
    if(!fyto_audit_enabled())return;
    WideCharToMultiByte(CP_UTF8,0,requested,-1,a,sizeof(a),NULL,NULL);
    WideCharToMultiByte(CP_UTF8,0,shown,-1,b,sizeof(b),NULL,NULL);
    count=sprintf_s(line,sizeof(line),"%u\t%lld\t%lld\t",message,begin.QuadPart,end.QuadPart);
    if(count>0)WriteFile(fyto_audit_file,line,(DWORD)count,&written,NULL);
    WriteFile(fyto_audit_file,a,(DWORD)strlen(a),&written,NULL);WriteFile(fyto_audit_file,"\t",1,&written,NULL);
    WriteFile(fyto_audit_file,b,(DWORD)strlen(b),&written,NULL);WriteFile(fyto_audit_file,"\n",1,&written,NULL);
}
#endif
