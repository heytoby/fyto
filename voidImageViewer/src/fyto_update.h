#include <wchar.h>
// No startup checks: the sole caller is Help -> Check for updates.
static void fyto_check_updates(HWND owner)
{
    wchar_t temp[MAX_PATH],folder[MAX_PATH],path[MAX_PATH],exe[STRING_SIZE],config[STRING_SIZE],repository[256],args[STRING_SIZE+512];
    GUID guid;HRSRC resource;HGLOBAL bytes;HANDLE file;DWORD size,written;SHELLEXECUTEINFOW launch;
    if(config_appdata){if(!string_get_appdata_voidimageviewer_path(config))return;}
    else string_get_exe_path(config);
    string_path_combine_utf8(config,config,(const utf8_t *)"updates.ini");
    GetPrivateProfileStringW(L"updates",L"repository",L"heytoby/fyto",repository,256,config);
    if(!GetTempPathW(MAX_PATH,temp) || FAILED(CoCreateGuid(&guid)))return;
    swprintf_s(folder,MAX_PATH,L"%sfyto-update-%08lx-%08lx",temp,GetCurrentProcessId(),guid.Data1);
    if(!CreateDirectoryW(folder,NULL))return;
    swprintf_s(path,MAX_PATH,L"%s\\fyto-updater.exe",folder);
    resource=FindResourceW(os_hinstance,MAKEINTRESOURCEW(60020),RT_RCDATA);
    bytes=resource?LoadResource(os_hinstance,resource):NULL;size=resource?SizeofResource(os_hinstance,resource):0;
    if(!bytes || !size)goto failed;
    file=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)goto failed;
    if(!WriteFile(file,LockResource(bytes),size,&written,NULL) || written!=size){CloseHandle(file);goto failed;}
    CloseHandle(file);
    GetModuleFileNameW(NULL,exe,STRING_SIZE);
    // Validate repository before quoting it into the child command line.
    {const wchar_t *p=repository;for(;*p;p++)if(!((*p>=L'a'&&*p<=L'z')||(*p>=L'A'&&*p<=L'Z')||(*p>=L'0'&&*p<=L'9')||*p==L'/'||*p==L'-'||*p==L'_'||*p==L'.'))goto failed;}
    swprintf_s(args,STRING_SIZE+512,L"\"%s\" \"%s\"",exe,repository);
    ZeroMemory(&launch,sizeof(launch));launch.cbSize=sizeof(launch);launch.hwnd=owner;launch.lpFile=path;launch.lpParameters=args;launch.nShow=SW_SHOWNORMAL;
    if(ShellExecuteExW(&launch))return;
failed:
    DeleteFileW(path);RemoveDirectoryW(folder);
    MessageBoxW(owner,L"无法启动更新程序。请确认已安装 .NET Framework 4.8，或从 GitHub 下载新版安装包。",L"fyto · 检查更新",MB_OK|MB_ICONERROR);
}
