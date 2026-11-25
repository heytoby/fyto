#define UNICODE
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include "../voidImageViewer/src/association_path.h"
int wmain(void)
{
    wchar_t temp[MAX_PATH],root[MAX_PATH],a[MAX_PATH],b[MAX_PATH],alias[MAX_PATH],missing[MAX_PATH],old_dir[MAX_PATH];
    HANDLE f;DWORD n,before,after;int i;
    assert(GetTempPathW(MAX_PATH,temp));
    assert(GetTempFileNameW(temp,L"fyt",0,root));assert(DeleteFileW(root));assert(CreateDirectoryW(root,NULL));
    swprintf_s(a,MAX_PATH,L"%s\\fyto.exe",root);
    swprintf_s(old_dir,MAX_PATH,L"%s\\old",root);assert(CreateDirectoryW(old_dir,NULL));
    swprintf_s(b,MAX_PATH,L"%s\\fyto.exe",old_dir);
    swprintf_s(alias,MAX_PATH,L"%s\\alias.exe",root);
    swprintf_s(missing,MAX_PATH,L"%s\\missing.exe",root);
    f=CreateFileW(a,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);assert(f!=INVALID_HANDLE_VALUE);
    assert(WriteFile(f,"same bytes",10,&n,NULL) && n==10);assert(CloseHandle(f));
    assert(CopyFileW(a,b,TRUE));assert(CreateHardLinkW(alias,a,NULL));
    assert(fyto_same_executable_file(a,a));assert(fyto_same_executable_file(a,alias));
    assert(!fyto_same_executable_file(a,b));assert(!fyto_same_executable_file(a,missing));
    assert(!fyto_same_executable_file(NULL,a));assert(!fyto_same_executable_file(L"",a));
    swprintf_s(missing,MAX_PATH,L"%s\\..\\%s",root,wcsrchr(root,L'\\')+1);
    wcscat_s(missing,MAX_PATH,L"\\fyto.exe");assert(fyto_same_executable_file(a,missing));
    assert(GetProcessHandleCount(GetCurrentProcess(),&before));
    for(i=0;i<1000;i++){assert(fyto_same_executable_file(a,alias));assert(!fyto_same_executable_file(a,b));}
    assert(GetProcessHandleCount(GetCurrentProcess(),&after));assert(before==after);
    assert(DeleteFileW(alias));assert(DeleteFileW(b));assert(RemoveDirectoryW(old_dir));assert(DeleteFileW(a));assert(RemoveDirectoryW(root));
    puts("Association executable identity: current file, aliases, old copies, missing files and 2000 handle cycles passed");
    return 0;
}
