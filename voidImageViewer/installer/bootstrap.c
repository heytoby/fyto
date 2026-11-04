#define UNICODE
#define _UNICODE
#include <windows.h>
#include <msi.h>
#include <msiquery.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <objbase.h>
#include <wchar.h>
#include <stdio.h>
#include "close-running.h"

static BOOL registry_path(HKEY root, REGSAM view, const wchar_t *key, const wchar_t *name, wchar_t *out)
{
    HKEY h; DWORD type=0,bytes=MAX_PATH*sizeof(wchar_t);
    if(RegOpenKeyExW(root,key,0,KEY_QUERY_VALUE|view,&h)!=ERROR_SUCCESS)return FALSE;
    LONG e=RegQueryValueExW(h,name,NULL,&type,(BYTE *)out,&bytes);RegCloseKey(h);
    return e==ERROR_SUCCESS && (type==REG_SZ || type==REG_EXPAND_SZ) && out[0];
}
static int legacy_view;
static void previous_directory(wchar_t *out)
{
    int i,j; HKEY roots[2]={HKEY_LOCAL_MACHINE,HKEY_CURRENT_USER};
    REGSAM views[2]={KEY_WOW64_64KEY,KEY_WOW64_32KEY}; wchar_t raw[MAX_PATH];
    out[0]=0;
    for(i=0;i<2;i++)for(j=0;j<2;j++) {
        if(registry_path(roots[i],views[j],L"Software\\voidImageViewer",L"InstallLocation",raw) && PathIsDirectoryW(raw)){wcscpy_s(out,MAX_PATH,raw);return;}
        if(registry_path(roots[i],views[j],L"Software\\fyto",L"InstallLocation",raw) && PathIsDirectoryW(raw)){wcscpy_s(out,MAX_PATH,raw);return;}
        if(registry_path(roots[i],views[j],L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\fyto",L"InstallLocation",raw)) {
            if(PathIsDirectoryW(raw)){legacy_view=i==0?(j==0?64:32):0;wcscpy_s(out,MAX_PATH,raw);return;}
        }
        if(registry_path(roots[i],views[j],L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\fyto",L"UninstallString",raw)) {
            wchar_t *start=raw,*end;
            if(*start==L'"'){start++;end=wcschr(start,L'"');if(end)*end=0;}
            if(PathFileExistsW(start)) {
                PathRemoveFileSpecW(start);
                if(PathIsDirectoryW(start)){legacy_view=i==0?(j==0?64:32):0;wcscpy_s(out,MAX_PATH,start);return;}
            }
        }
    }
}
static BOOL product_code(MSIHANDLE db,wchar_t *out)
{
    MSIHANDLE view=0,row=0; DWORD len=64; BOOL ok=FALSE;
    if(MsiDatabaseOpenViewW(db,L"SELECT `Value` FROM `Property` WHERE `Property`='ProductCode'",&view)==ERROR_SUCCESS &&
       MsiViewExecute(view,0)==ERROR_SUCCESS && MsiViewFetch(view,&row)==ERROR_SUCCESS)
        ok=MsiRecordGetStringW(row,1,out,&len)==ERROR_SUCCESS;
    if(row)MsiCloseHandle(row);if(view)MsiCloseHandle(view);return ok;
}
static void launch_format_setup(const wchar_t *product)
{
    wchar_t directory[MAX_PATH],exe[MAX_PATH];DWORD size=MAX_PATH;SHELLEXECUTEINFOW info;
    directory[0]=0;
    if(MsiGetProductInfoW(product,INSTALLPROPERTY_INSTALLLOCATION,directory,&size)!=ERROR_SUCCESS || !directory[0])
        registry_path(HKEY_LOCAL_MACHINE,KEY_WOW64_64KEY,L"Software\\fyto",L"InstallLocation",directory);
    if(!directory[0] || !PathCombineW(exe,directory,L"fyto.exe") || !PathFileExistsW(exe)){
        MessageBoxW(NULL,L"安装已完成。请启动 fyto，在“帮助 → 图片格式关联”中选择默认图片格式。",L"fyto",MB_OK|MB_ICONINFORMATION);return;
    }
    ZeroMemory(&info,sizeof(info));info.cbSize=sizeof(info);info.lpFile=exe;info.lpParameters=L"/setup /window";
    info.lpDirectory=directory;info.nShow=SW_SHOWNORMAL;
    if(!ShellExecuteExW(&info))MessageBoxW(NULL,L"安装已完成，但未能启动格式选择。请打开 fyto，在“帮助 → 图片格式关联”中重新选择。",L"fyto",MB_OK|MB_ICONINFORMATION);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE prev,LPWSTR command,int show)
{
    wchar_t temp[MAX_PATH],dir[MAX_PATH],msi[MAX_PATH],id[64],target[MAX_PATH],args[1024],code[64],message[512];
    GUID guid; HRSRC res; HGLOBAL loaded; HANDLE file; DWORD count,size; UINT result=ERROR_INSTALL_FAILURE;
    MSIHANDLE db=0; BOOL verify=wcscmp(command,L"/verify")==0; BOOL updating=wcscmp(command,L"/update")==0;
    (void)prev;(void)show;
    if(wcsncmp(command,L"/close-running ",15)==0){DWORD pid=wcstoul(command+15,NULL,10);return pid && close_running_fyto(pid)?0:1;}
    if(!GetTempPathW(MAX_PATH,temp) || FAILED(CoCreateGuid(&guid)))return ERROR_PATH_NOT_FOUND;
    StringFromGUID2(&guid,id,64);
    if(swprintf_s(dir,MAX_PATH,L"%sviv-setup-%s",temp,id)<0 || !CreateDirectoryW(dir,NULL))return ERROR_PATH_NOT_FOUND;
    swprintf_s(msi,MAX_PATH,L"%s\\viewer.msi",dir);
    res=FindResourceW(instance,MAKEINTRESOURCEW(101),RT_RCDATA);
    loaded=res?LoadResource(instance,res):NULL;size=res?SizeofResource(instance,res):0;
    if(!loaded || !size)goto cleanup;
    file=CreateFileW(msi,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)goto cleanup;
    if(!WriteFile(file,LockResource(loaded),size,&count,NULL) || count!=size){CloseHandle(file);goto cleanup;}
    CloseHandle(file);
    if(MsiOpenDatabaseW(msi,MSIDBOPEN_READONLY,&db)!=ERROR_SUCCESS || !product_code(db,code))goto cleanup;
    MsiCloseHandle(db);db=0;
    if(verify){result=ERROR_SUCCESS;goto cleanup;}
    if(!close_running_fyto(0)){result=ERROR_INSTALL_ALREADY_RUNNING;goto cleanup;}
    previous_directory(target);
    wcscpy_s(args,1024,L"REINSTALLMODE=amus REBOOT=ReallySuppress");
    if(target[0]){wcscat_s(args,1024,L" INSTALLFOLDER=\"");wcscat_s(args,1024,target);wcscat_s(args,1024,L"\"");}
    if(legacy_view && target[0]){
        wcscat_s(args,1024,legacy_view==64?L" OLDVIEW=64 OLDINSTALLDIR=\"":L" OLDVIEW=32 OLDINSTALLDIR=\"");
        wcscat_s(args,1024,target);
        if(target[wcslen(target)-1]!=L'\\')wcscat_s(args,1024,L"\\");
        wcscat_s(args,1024,L"\"");
    }
    if(MsiQueryProductStateW(code)==INSTALLSTATE_DEFAULT)wcscat_s(args,1024,L" REINSTALL=ALL");
    MsiSetInternalUI(updating?INSTALLUILEVEL_NONE:INSTALLUILEVEL_FULL,NULL);
    result=MsiInstallProductW(msi,args);
    if(!updating && (result==ERROR_SUCCESS || result==ERROR_SUCCESS_REBOOT_REQUIRED) && MsiQueryProductStateW(code)==INSTALLSTATE_DEFAULT)launch_format_setup(code);
cleanup:
    if(db)MsiCloseHandle(db);
    DeleteFileW(msi);RemoveDirectoryW(dir);
    if(!verify && result!=ERROR_SUCCESS && result!=ERROR_SUCCESS_REBOOT_REQUIRED && result!=ERROR_INSTALL_USEREXIT) {
        swprintf_s(message,512,L"安装未完成，错误代码：%u。原有图片不会被修改。",result);
        MessageBoxW(NULL,message,L"fyto 安装",MB_OK|MB_ICONERROR);
    }
    return (int)result;
}
