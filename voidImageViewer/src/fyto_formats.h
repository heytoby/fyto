#include <stdlib.h>
#include "format_samples.h"

static const wchar_t *fyto_extensions[]={L".jpg",L".jpeg",L".png",L".webp",L".gif",L".bmp",L".tif",L".tiff",L".ico",L".arw"};
#define FYTO_FORMAT_COUNT (sizeof(fyto_extensions)/sizeof(fyto_extensions[0]))
#define FYTO_ASSOCIATION_KEY L"Software\\fyto\\Associations"
static int fyto_formats_requested,fyto_startup_complete,fyto_offer_formats;
static HWND fyto_formats_window;

static BOOL fyto_association_file(wchar_t *path,BOOL create)
{
    if(config_appdata){
        if(!string_get_appdata_voidimageviewer_path(path))return FALSE;
        if(create)CreateDirectoryW(path,NULL);
    }else string_get_exe_path(path);
    string_path_combine_utf8(path,path,(const utf8_t *)"associations.ini");
    return TRUE;
}
static BOOL fyto_read_association_value(const wchar_t *name,DWORD *value)
{
    HKEY key;DWORD type=0,size=sizeof(*value);LONG result;
    wchar_t path[STRING_SIZE],text[32],*end;
    if(fyto_association_file(path,FALSE) && GetPrivateProfileStringW(L"fyto",name,L"",text,32,path)){
        DWORD parsed=wcstoul(text,&end,10);
        if(end!=text && !*end){*value=parsed;return TRUE;}
    }
    // Preserve preferences saved by older fyto releases.
    if(RegOpenKeyExW(HKEY_CURRENT_USER,FYTO_ASSOCIATION_KEY,0,KEY_QUERY_VALUE,&key)!=ERROR_SUCCESS)return FALSE;
    result=RegQueryValueExW(key,name,NULL,&type,(BYTE *)value,&size);RegCloseKey(key);
    return result==ERROR_SUCCESS && type==REG_DWORD && size==sizeof(*value);
}
static BOOL fyto_write_association_value(const wchar_t *name,DWORD value)
{
    HKEY key;LONG result;wchar_t path[STRING_SIZE],text[32];
    wsprintfW(text,L"%u",value);
    if(fyto_association_file(path,TRUE) && WritePrivateProfileStringW(L"fyto",name,text,path))return TRUE;
    if(RegCreateKeyExW(HKEY_CURRENT_USER,FYTO_ASSOCIATION_KEY,0,NULL,0,KEY_SET_VALUE,NULL,&key,NULL)!=ERROR_SUCCESS)return FALSE;
    result=RegSetValueExW(key,name,0,REG_DWORD,(const BYTE *)&value,sizeof(value));RegCloseKey(key);
    return result==ERROR_SUCCESS;
}
static int fyto_is_default(const wchar_t *ext)
{
    wchar_t exe[STRING_SIZE];DWORD len=STRING_SIZE;
    return SUCCEEDED(AssocQueryStringW(ASSOCF_NONE,ASSOCSTR_EXECUTABLE,ext,L"open",exe,&len)) && lstrcmpiW(PathFindFileNameW(exe),L"fyto.exe")==0;
}
static BOOL fyto_reg_string(const wchar_t *key,const wchar_t *name,const wchar_t *value)
{
    HKEY h;LONG result;
    if(RegCreateKeyExW(HKEY_CURRENT_USER,key,0,NULL,0,KEY_SET_VALUE,NULL,&h,NULL)!=ERROR_SUCCESS)return FALSE;
    result=RegSetValueExW(h,name,0,REG_SZ,(const BYTE *)value,((DWORD)wcslen(value)+1)*sizeof(wchar_t));RegCloseKey(h);
    return result==ERROR_SUCCESS;
}
static BOOL fyto_register_formats(void)
{
    wchar_t exe[STRING_SIZE],command[STRING_SIZE+16],key[256];unsigned i;BOOL ok=TRUE;
    GetModuleFileNameW(NULL,exe,STRING_SIZE);wsprintfW(command,L"\"%s\" \"%%1\"",exe);
    ok&=fyto_reg_string(L"Software\\Classes\\fyto.Image\\shell\\open\\command",NULL,command);
    ok&=fyto_reg_string(L"Software\\Classes\\Applications\\fyto.exe\\shell\\open\\command",NULL,command);
    ok&=fyto_reg_string(L"Software\\Classes\\Applications\\fyto.exe",L"FriendlyAppName",L"fyto");
    ok&=fyto_reg_string(L"Software\\Classes\\fyto.Image",NULL,L"fyto Image");
    wsprintfW(command,L"\"%s\",0",exe);
    ok&=fyto_reg_string(L"Software\\Classes\\fyto.Image\\DefaultIcon",NULL,command);
    ok&=fyto_reg_string(L"Software\\RegisteredApplications",L"fyto",L"Software\\fyto\\Capabilities");
    ok&=fyto_reg_string(L"Software\\fyto\\Capabilities",L"ApplicationName",L"fyto");
    ok&=fyto_reg_string(L"Software\\fyto\\Capabilities",L"ApplicationDescription",L"fyto 图片浏览器");
    for(i=0;i<FYTO_FORMAT_COUNT;i++){
        HKEY h;LONG result;
        ok&=fyto_reg_string(L"Software\\Classes\\Applications\\fyto.exe\\SupportedTypes",fyto_extensions[i],L"");
        ok&=fyto_reg_string(L"Software\\fyto\\Capabilities\\FileAssociations",fyto_extensions[i],L"fyto.Image");
        wsprintfW(key,L"Software\\Classes\\%s\\OpenWithProgids",fyto_extensions[i]);
        result=RegCreateKeyExW(HKEY_CURRENT_USER,key,0,NULL,0,KEY_SET_VALUE,NULL,&h,NULL);
        if(result==ERROR_SUCCESS){result=RegSetValueExW(h,L"fyto.Image",0,REG_NONE,NULL,0);RegCloseKey(h);}ok&=result==ERROR_SUCCESS;
    }
    SHChangeNotify(SHCNE_ASSOCCHANGED,SHCNF_IDLIST,NULL,NULL);
    return ok;
}
static DWORD fyto_checked_formats(HWND hwnd)
{
    DWORD mask=0;unsigned i;
    for(i=0;i<FYTO_FORMAT_COUNT;i++)if(IsDlgButtonChecked(hwnd,61000+i)==BST_CHECKED)mask|=1u<<i;
    return mask;
}
static BOOL fyto_save_format_selection(HWND hwnd)
{
    // Store the user's selection independently of INI location and installer files.
    if(!fyto_write_association_value(L"SelectedFormats",fyto_checked_formats(hwnd)) ||
       !fyto_write_association_value(L"SetupVersion",1)){
        MessageBoxW(hwnd,L"未能保存格式选择，请检查当前用户设置的写入权限后重试。",L"fyto",MB_OK|MB_ICONERROR);return FALSE;
    }
    fyto_offer_formats=0;
    return TRUE;
}
static HWND fyto_format_control(HWND owner,const wchar_t *cls,const wchar_t *text,DWORD style,int id,int x,int y,int width,int height)
{
    RECT r={x,y,x+width,y+height};HWND child;
    MapDialogRect(owner,&r);
    child=CreateWindowW(cls,text,WS_CHILD|WS_VISIBLE|style,r.left,r.top,r.right-r.left,r.bottom-r.top,owner,(HMENU)(INT_PTR)id,GetModuleHandleW(NULL),NULL);
    SendMessageW(child,WM_SETFONT,SendMessageW(owner,WM_GETFONT,0,0),TRUE);return child;
}
static void fyto_refresh_formats(HWND hwnd)
{
    unsigned i;int count=0;wchar_t text[160];
    for(i=0;i<FYTO_FORMAT_COUNT;i++){
        BOOL current=fyto_is_default(fyto_extensions[i]);count+=current;
        SetDlgItemTextW(hwnd,61100+i,current?L"已是默认":L"设为默认");
        EnableWindow(GetDlgItem(hwnd,61100+i),!current);
    }
    wsprintfW(text,L"当前有 %d 种格式默认由 fyto 打开。保存勾选不会替代系统确认。",count);
    SetDlgItemTextW(hwnd,61200,text);
}
static BOOL fyto_is_setup_sample(const wchar_t *filename)
{
    wchar_t path[STRING_SIZE],marker[STRING_SIZE],name[MAX_PATH];
    if(!GetFullPathNameW(filename,STRING_SIZE,path,NULL))return FALSE;
    lstrcpynW(name,PathFindFileNameW(path),MAX_PATH);PathRemoveExtensionW(name);
    if(lstrcmpiW(name,L"fyto-default"))return FALSE;
    lstrcpyW(marker,path);PathRemoveFileSpecW(marker);lstrcatW(marker,L"\\.fyto-format-setup");
    return GetFileAttributesW(marker)!=INVALID_FILE_ATTRIBUTES;
}
static BOOL fyto_is_setup_sample_launch(void)
{
    wchar_t word[STRING_SIZE];wchar_t *p=GetCommandLineW();
    p=string_get_word(string_skip_ws(p),word);
    p=string_get_word(string_skip_ws(p),word);
    return word[0] && !*string_skip_ws(p) && fyto_is_setup_sample(word);
}
static int fyto_open_format_picker(HWND owner,unsigned index)
{
    wchar_t path[STRING_SIZE],directory[MAX_PATH];SHELLEXECUTEINFOW info;
    if(index==FYTO_FORMAT_COUNT-1){
        OPENFILENAMEW ofn;ZeroMemory(&ofn,sizeof(ofn));path[0]=0;ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=owner;
        ofn.lpstrTitle=L"选择一张 ARW 图片以设置默认打开方式";ofn.lpstrFilter=L"Sony RAW (*.arw)\0*.arw\0\0";
        ofn.lpstrFile=path;ofn.nMaxFile=STRING_SIZE;ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
        if(!GetOpenFileNameW(&ofn))return CommDlgExtendedError()?-1:0;
    }else{
        HANDLE file;DWORD written;BOOL ok;
        if(!GetTempPathW(MAX_PATH,directory))return -1;
        if(!GetTempFileNameW(directory,L"fyt",0,path))return -1;
        DeleteFileW(path);
        if(!CreateDirectoryW(path,NULL))return -1;
        {
            wchar_t marker[STRING_SIZE];HANDLE tag;
            lstrcpyW(marker,path);lstrcatW(marker,L"\\.fyto-format-setup");
            tag=CreateFileW(marker,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_HIDDEN,NULL);
            if(tag==INVALID_HANDLE_VALUE)return -1;
            CloseHandle(tag);
        }
        lstrcatW(path,L"\\fyto-default");lstrcatW(path,fyto_extensions[index]);
        file=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_TEMPORARY,NULL);
        if(file==INVALID_HANDLE_VALUE)return -1;
        ok=WriteFile(file,fyto_format_samples[index].data,fyto_format_samples[index].size,&written,NULL) && written==fyto_format_samples[index].size;CloseHandle(file);
        if(!ok){DeleteFileW(path);return -1;}
    }
    // The system's Open With picker provides the user's Always choice.
    // No protected UserChoice data is written and no Settings page is launched.
    ZeroMemory(&info,sizeof(info));info.cbSize=sizeof(info);info.hwnd=owner;info.lpVerb=L"openas";info.lpFile=path;
    info.fMask=SEE_MASK_NOASYNC|SEE_MASK_NOCLOSEPROCESS|SEE_MASK_INVOKEIDLIST;info.nShow=SW_SHOWNORMAL;
    if(!ShellExecuteExW(&info))return -1;
    if(info.hProcess)CloseHandle(info.hProcess);
    return TRUE;
}
static DWORD fyto_initial_format_selection(void)
{
    DWORD mask=0;unsigned i;
    if(!fyto_read_association_value(L"SelectedFormats",&mask)){
        for(i=0;i<FYTO_FORMAT_COUNT;i++)if(fyto_is_default(fyto_extensions[i]))mask|=1u<<i;
        if(!mask)mask=15;
    }
    return mask&((1u<<FYTO_FORMAT_COUNT)-1);
}
static INT_PTR CALLBACK fyto_formats_proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp)
{
    unsigned i;
    if(msg==WM_INITDIALOG){
        DWORD mask=fyto_initial_format_selection();
        fyto_formats_window=hwnd;
        SetWindowTextW(hwnd,L"将 fyto 设为默认图片浏览器");
        fyto_format_control(hwnd,L"STATIC",L"可一键勾选全部格式，或自行选择。设置默认时，请在打开方式中选择 fyto 并点击“始终”。",0,0,12,10,352,30);
        fyto_format_control(hwnd,L"BUTTON",L"一键全选",WS_TABSTOP,61201,12,42,75,18);
        fyto_format_control(hwnd,L"BUTTON",L"全部取消",WS_TABSTOP,61202,94,42,75,18);
        for(i=0;i<FYTO_FORMAT_COUNT;i++){
            int x=12+(i%2)*181,y=68+(i/2)*31;
            fyto_format_control(hwnd,L"BUTTON",fyto_extensions[i],WS_TABSTOP|BS_AUTOCHECKBOX,61000+i,x,y,67,20);
            CheckDlgButton(hwnd,61000+i,(mask&(1u<<i))?BST_CHECKED:BST_UNCHECKED);
            fyto_format_control(hwnd,L"BUTTON",L"设为默认",WS_TABSTOP,61100+i,x+70,y,92,20);
        }
        fyto_format_control(hwnd,L"STATIC",L"勾选会保留到下次打开和升级安装。取消勾选不会更改现有默认程序。",0,0,12,228,352,26);
        fyto_format_control(hwnd,L"STATIC",L"",0,61200,12,256,352,26);
        fyto_format_control(hwnd,L"BUTTON",L"保存选择",WS_TABSTOP|BS_DEFPUSHBUTTON,IDOK,190,288,80,22);
        fyto_format_control(hwnd,L"BUTTON",L"暂不设置",WS_TABSTOP,IDCANCEL,282,288,80,22);
        fyto_refresh_formats(hwnd);SetTimer(hwnd,1,1000,NULL);return TRUE;
    }
    if(msg==WM_TIMER){fyto_refresh_formats(hwnd);return TRUE;}
    if(msg==WM_COMMAND){
        int id=LOWORD(wp);
        if(id==61201 || id==61202){
            for(i=0;i<FYTO_FORMAT_COUNT;i++)CheckDlgButton(hwnd,61000+i,id==61201?BST_CHECKED:BST_UNCHECKED);
            return TRUE;
        }
        if(id==IDCANCEL){fyto_write_association_value(L"SetupVersion",1);EndDialog(hwnd,0);return TRUE;}
        if(id==IDOK){if(fyto_save_format_selection(hwnd))EndDialog(hwnd,1);return TRUE;}
        if(id>=61100 && id<61100+FYTO_FORMAT_COUNT){
            i=id-61100;CheckDlgButton(hwnd,61000+i,BST_CHECKED);
            if(fyto_save_format_selection(hwnd)){
                if(!fyto_register_formats())MessageBoxW(hwnd,L"未能注册打开方式，请检查当前用户的写入权限。",L"fyto",MB_OK|MB_ICONERROR);
                else if(fyto_open_format_picker(hwnd,i)<0)MessageBoxW(hwnd,L"未能打开格式选择，请重试。",L"fyto",MB_OK|MB_ICONERROR);
            }
            fyto_refresh_formats(hwnd);return TRUE;
        }
    }
    if(msg==WM_CLOSE){fyto_write_association_value(L"SetupVersion",1);EndDialog(hwnd,0);return TRUE;}
    if(msg==WM_DESTROY){KillTimer(hwnd,1);return TRUE;}
    return FALSE;
}
static void fyto_format_dialog(HWND owner)
{
#pragma pack(push,2)
    struct {DLGTEMPLATE dlg;WORD menu,cls,title,point;wchar_t font[9];} data;
#pragma pack(pop)
    if(fyto_formats_window && IsWindow(fyto_formats_window)){SetForegroundWindow(fyto_formats_window);return;}
    ZeroMemory(&data,sizeof(data));data.dlg.style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|DS_CENTER|DS_SETFONT;data.dlg.cx=376;data.dlg.cy=324;
    data.point=9;lstrcpyW(data.font,L"Segoe UI");
    // Mark only inside the displayed dialog, allowing a retry after creation failure.
    DialogBoxIndirectParamW(GetModuleHandleW(NULL),&data.dlg,owner,fyto_formats_proc,0);
    fyto_formats_window=NULL;
    InvalidateRect(owner,NULL,FALSE);
}
