#include <stdlib.h>
// Defined in titlebar.h, which is included after this header.
#include "format_samples.h"

static const wchar_t *fyto_extensions[]={L".jpg",L".jpeg",L".png",L".webp",L".gif",L".bmp",L".tif",L".tiff",L".ico",L".arw"};
#define FYTO_FORMAT_COUNT (sizeof(fyto_extensions)/sizeof(fyto_extensions[0]))
#define FYTO_ASSOCIATION_KEY L"Software\\fyto\\Associations"
static int fyto_formats_requested,fyto_startup_complete,fyto_offer_formats;
// Set when the dialog is opened automatically after an install, so it can
// stay quiet when every offered format already points at fyto.
static int fyto_formats_auto;
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
    wchar_t current[STRING_SIZE];
    GetModuleFileNameW(NULL,current,ARRAYSIZE(current));
    return SUCCEEDED(AssocQueryStringW(ASSOCF_NONE,ASSOCSTR_EXECUTABLE,ext,L"open",exe,&len)) &&
        lstrcmpiW(exe,current)==0;
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
    for(i=0;i+1<FYTO_FORMAT_COUNT;i++){
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


static HWND fyto_format_control(HWND owner,const wchar_t *cls,const wchar_t *text,DWORD style,int id,int x,int y,int width,int height)
{
    RECT r={x,y,x+width,y+height};HWND child;
    MapDialogRect(owner,&r);
    child=CreateWindowW(cls,text,WS_CHILD|WS_VISIBLE|style,r.left,r.top,r.right-r.left,r.bottom-r.top,owner,(HMENU)(INT_PTR)id,GetModuleHandleW(NULL),NULL);
    SendMessageW(child,WM_SETFONT,SendMessageW(owner,WM_GETFONT,0,0),TRUE);return child;
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

static DWORD fyto_initial_format_selection(void)
{
    DWORD mask=0;unsigned i;
    if(!fyto_read_association_value(L"SelectedFormats",&mask)){
        for(i=0;i<FYTO_FORMAT_COUNT;i++)if(fyto_is_default(fyto_extensions[i]))mask|=1u<<i;
        if(!mask)mask=15;
    }
    return mask&((1u<<FYTO_FORMAT_COUNT)-1);
}
static BOOL fyto_offered_format(unsigned index)
{
    // .arw stays registered for Open With, but fyto cannot decode Sony RAW,
    // so it is not offered as a default format.
    return index + 1 < FYTO_FORMAT_COUNT;
}
static unsigned fyto_missing_format_count(void)
{
    DWORD mask=fyto_initial_format_selection();unsigned i,n=0;
    for(i=0;i<FYTO_FORMAT_COUNT;i++)
        if(fyto_offered_format(i) && (mask&(1u<<i)) && !fyto_is_default(fyto_extensions[i]))n++;
    return n;
}

// Windows signs UserChoice, so a checked box can only be applied by letting the
// user confirm that format once in the system Open With dialog.
static void fyto_sync_boxes(HWND hwnd)
{
    unsigned i;
    for (i=0; i<FYTO_FORMAT_COUNT; i++)
    {
        wchar_t label[100];
        if (!fyto_offered_format(i)) continue;
        wsprintfW(label,L"%s    %s",fyto_extensions[i],
            fyto_is_default(fyto_extensions[i]) ? L"已设为默认" : L"尚未设为默认");
        SetDlgItemTextW(hwnd,61000+i,label);
    }
}
static INT_PTR CALLBACK fyto_formats_proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp)
{
    unsigned i;
    if (msg==WM_INITDIALOG)
    {
        fyto_formats_window=hwnd;
        SetWindowTextW(hwnd,L"fyto · 默认图片浏览器");
        fyto_format_control(hwnd,L"BUTTON",L"打开系统默认应用设置",WS_TABSTOP|BS_DEFPUSHBUTTON,61202,12,10,230,26);
        fyto_format_control(hwnd,L"STATIC",L"在 Windows 设置中选择 fyto，并确认需要关联的图片格式。",0,0,12,46,352,20);
        for (i=0;i<FYTO_FORMAT_COUNT;i++)
        {
            int x,y;
            if (!fyto_offered_format(i)) continue;
            x=12+(i%2)*181;
            y=76+(i/2)*23;
            fyto_format_control(hwnd,L"STATIC",L"",0,61000+i,x,y,175,20);
        }
        fyto_format_control(hwnd,L"BUTTON",L"刷新状态",WS_TABSTOP,61201,12,198,90,22);
        fyto_format_control(hwnd,L"BUTTON",L"关闭",WS_TABSTOP,IDCANCEL,286,198,80,22);
        fyto_sync_boxes(hwnd);
        return TRUE;
    }
    if (msg==WM_ACTIVATE && LOWORD(wp)!=WA_INACTIVE)
    {
        fyto_sync_boxes(hwnd);
        return TRUE;
    }
    if (msg==WM_COMMAND)
    {
        int id=LOWORD(wp);
        if (id==61202)
        {
            INT_PTR launched;
            if (!fyto_register_formats())
            {
                MessageBoxW(hwnd,L"无法注册 fyto 的图片打开方式，请检查当前用户的写入权限。",L"fyto",MB_OK|MB_ICONERROR);
                return TRUE;
            }
            launched=(INT_PTR)ShellExecuteW(hwnd,L"open",
                L"ms-settings:defaultapps?registeredAppUser=fyto",NULL,NULL,SW_SHOWNORMAL);
            if (launched<=32)
                ShellExecuteW(hwnd,L"open",L"ms-settings:defaultapps",NULL,NULL,SW_SHOWNORMAL);
            return TRUE;
        }
        if (id==61201) { fyto_sync_boxes(hwnd); return TRUE; }
        if (id==IDCANCEL)
        {
            fyto_write_association_value(L"SetupVersion",1);
            EndDialog(hwnd,0);
            return TRUE;
        }
    }
    if (msg==WM_CLOSE)
    {
        fyto_write_association_value(L"SetupVersion",1);
        EndDialog(hwnd,0);
        return TRUE;
    }
    return FALSE;
}
// Hand-built dialog template: the byte count has to match the real layout
// exactly. The old version declared a 44-byte struct but copied a 38-byte font
// name into an 18-byte field, so the string ran 20 bytes past the end of the
// template and the dialog manager then read its NUL from beyond the struct.
#define FYTO_FORMATS_FONT L"Microsoft YaHei UI"
#pragma pack(push,1)
typedef struct _fyto_formats_dlg_template_s
{
    DLGTEMPLATE dlg;        /* 18 bytes, no padding */
    WORD menu,cls,title;    /* no menu, dialog class, no title text */
    WORD point;             /* DS_SETFONT size */
    wchar_t font[ARRAYSIZE(FYTO_FORMATS_FONT)]; /* typeface, NUL terminated */
} fyto_formats_dlg_template_t;
#pragma pack(pop)
// Compile-time proof that the font field holds the whole name.
typedef char _fyto_formats_font_fits[
    (ARRAYSIZE(FYTO_FORMATS_FONT) > 1 &&
     sizeof(((fyto_formats_dlg_template_t *)0)->font) == sizeof(FYTO_FORMATS_FONT)) ? 1 : -1];

static void fyto_format_dialog(HWND owner)
{
    // A union with a pointer keeps the template word-aligned on the stack.
    union { void *align; fyto_formats_dlg_template_t t; } tpl;
    if(fyto_formats_window && IsWindow(fyto_formats_window)){SetForegroundWindow(fyto_formats_window);return;}
    ZeroMemory(&tpl,sizeof(tpl));
    tpl.t.dlg.style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|DS_CENTER|DS_SETFONT;
    tpl.t.dlg.cx=376;tpl.t.dlg.cy=236;
    tpl.t.point=9;
    lstrcpyW(tpl.t.font,FYTO_FORMATS_FONT);
    DialogBoxIndirectParamW(GetModuleHandleW(NULL),&tpl.t.dlg,owner,fyto_formats_proc,0);
    fyto_formats_window=NULL;
    InvalidateRect(owner,NULL,FALSE);
}

// ---------------------------------------------------------------- startup check

// Formats the user claims for fyto but that Windows still opens elsewhere.
// Runs shortly after the window is shown. Windows 10/11 sign the per-user
// UserChoice key, so no program may switch the default silently; the only legal
// route is the "Open with" picker, which this prompt sends the user to.