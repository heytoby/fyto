from pathlib import Path
import subprocess,json
import tempfile
root=Path(__file__).resolve().parents[1];src=root/'voidImageViewer/src';out=Path(tempfile.mkdtemp(prefix='fyto-association-test-'))
formats=(src/'fyto_formats.h').read_text(encoding='utf-8-sig');brand=(src/'fyto_brand.h').read_text(encoding='utf-8-sig')
def function(s,name):
 at=s.index(name+'(');start=s.rfind('static ',0,at);at=s.index('{',at);depth=0
 for end in range(at,len(s)):
  if s[end]=='{':depth+=1
  elif s[end]=='}':
   depth-=1
   if not depth:return s[start:end+1]
 raise ValueError(name)
preamble=r'''
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <wchar.h>
#include <assert.h>
#include <stdio.h>
typedef uint32_t DWORD;typedef unsigned char BYTE;typedef int LONG;typedef int BOOL;typedef uintptr_t HKEY;typedef uintptr_t HWND;
#define TRUE 1
#define FALSE 0
#define HKEY_CURRENT_USER 1
#define KEY_QUERY_VALUE 1
#define KEY_SET_VALUE 2
#define ERROR_SUCCESS 0
#define REG_DWORD 4
#define BST_CHECKED 1
#define MB_OK 0
#define MB_ICONERROR 0
#define FYTO_ASSOCIATION_KEY L"Software\\fyto\\Associations"
#define FYTO_FORMAT_COUNT 10
static const wchar_t *fyto_extensions[]={L".jpg",L".jpeg",L".png",L".webp",L".gif",L".bmp",L".tif",L".tiff",L".ico",L".arw"};
static DWORD selected,version,checks,defaults;static int selected_exists,version_exists,fail_writes,wrong_type,dialogs,messages,old_file_checks;
static int fyto_formats_requested,fyto_startup_complete;
static LONG RegOpenKeyExW(HKEY root,const wchar_t *key,DWORD a,DWORD b,HKEY *h){assert(root==HKEY_CURRENT_USER);assert(!wcscmp(key,FYTO_ASSOCIATION_KEY));*h=1;return 0;}
static LONG RegCreateKeyExW(HKEY root,const wchar_t *key,DWORD a,void *b,DWORD c,DWORD d,void *e,HKEY *h,void *f){*h=1;return fail_writes?5:0;}
static LONG RegQueryValueExW(HKEY h,const wchar_t *name,void *unused,DWORD *type,BYTE *data,DWORD *size){int sel=!wcscmp(name,L"SelectedFormats");if(!(sel?selected_exists:version_exists))return 2;assert(*size==4);*type=wrong_type?1:REG_DWORD;*size=4;memcpy(data,sel?&selected:&version,4);return 0;}
static LONG RegSetValueExW(HKEY h,const wchar_t *name,DWORD unused,DWORD type,const BYTE *data,DWORD size){assert(type==REG_DWORD && size==4);if(fail_writes)return 5;if(!wcscmp(name,L"SelectedFormats")){selected_exists=1;memcpy(&selected,data,4);}else{assert(!wcscmp(name,L"SetupVersion"));version_exists=1;memcpy(&version,data,4);}return 0;}
static void RegCloseKey(HKEY h){}
static int IsDlgButtonChecked(HWND hwnd,int id){assert(id>=61000 && id<61010);return (checks&(1u<<(id-61000)))?BST_CHECKED:0;}
static int MessageBoxW(HWND hwnd,const wchar_t *text,const wchar_t *title,int flags){messages++;return 1;}
static void fyto_format_dialog(HWND owner){dialogs++;}
static int fyto_is_default(const wchar_t *ext){unsigned i;for(i=0;i<10;i++)if(!wcscmp(ext,fyto_extensions[i]))return !!(defaults&(1u<<i));assert(0);return 0;}
static DWORD GetFileAttributesW(const wchar_t *path){old_file_checks++;return 32;}
'''
functions='\n'.join(function(formats,n) for n in ['fyto_read_association_value','fyto_write_association_value','fyto_checked_formats','fyto_save_format_selection','fyto_initial_format_selection'])+'\n'+function(brand,'fyto_first_run')
tests=r'''
static void reset(void){selected=version=checks=defaults=0;selected_exists=version_exists=fail_writes=wrong_type=dialogs=messages=old_file_checks=0;fyto_formats_requested=fyto_startup_complete=0;}
int main(void){
 reset();fyto_first_run(1);assert(dialogs==1 && fyto_startup_complete && old_file_checks==0); // clean install and old marker/INI both reach the selector
 reset();version_exists=1;version=1;fyto_first_run(1);assert(dialogs==0); // routine startup retains dismissal
 fyto_formats_requested=1;fyto_first_run(1);assert(dialogs==1); // upgrade/repair /formats explicitly reopens
 reset();version_exists=1;version=0;fyto_first_run(1);assert(dialogs==1); // incomplete setup retried
 reset();version_exists=1;version=1;wrong_type=1;fyto_first_run(1);assert(dialogs==1); // malformed marker never silently skips
 reset();assert(fyto_initial_format_selection()==15);defaults=(1u<<2)|(1u<<9);assert(fyto_initial_format_selection()==defaults); // old actual associations preselected
 reset();for(DWORD mask=0;mask<1024;mask++){checks=mask;assert(fyto_save_format_selection(1));assert(selected==mask && version==1);defaults=1023^mask;assert(fyto_initial_format_selection()==mask);} // every saved combination, including no selection, survives conflicting Windows defaults
 fyto_first_run(1);assert(dialogs==0);
 reset();checks=5;fail_writes=1;assert(!fyto_save_format_selection(1));assert(!version_exists && messages==1);fyto_first_run(1);assert(dialogs==1); // failed writes remain retryable
 puts("association state: 1032 scenarios passed");return 0;
}
'''
p=out/'association_state_test.c';p.write_text(preamble+functions+tests)
subprocess.run(['gcc','-std=c11','-O2',str(p),'-o',str(out/'association_state_test')],check=True)
r=subprocess.run([str(out/'association_state_test')],capture_output=True,text=True);assert r.returncode==0,r.stderr
# Check the install launch and single-instance forwarding wiring in the real sources.
viv=(src/'viv.c').read_text(encoding='utf-8-sig');boot=(src.parent/'installer/bootstrap.c').read_text(encoding='utf-8-sig')
assert 'if(fyto_startup_complete)PostMessageW(_viv_hwnd,WM_COMMAND,60003,0);' in viv
assert 'case 60003: fyto_format_dialog(_viv_hwnd)' in viv
assert 'lpParameters=L"/formats"' in boot
assert 'if((result==ERROR_SUCCESS || result==ERROR_SUCCESS_REBOOT_REQUIRED) && MsiQueryProductStateW(code)==INSTALLSTATE_DEFAULT)launch_format_setup(code);' in boot
assert 'if(verify){result=ERROR_SUCCESS;goto cleanup;}' in boot
assert 'ms-settings:' not in formats and 'SHOpenWithDialog(' not in formats
report={'state_scenarios':1032,'legacy_ini_and_marker_do_not_suppress':True,'normal_startup_once':True,'forced_upgrade_and_manual_reopen':True,'saved_masks':1024,'saved_empty_mask_preserved':True,'failed_writes_retryable':True,'installer_success_launch_wired':True,'single_instance_forwarding_wired':True,'windows_settings_page_not_launched':True,'runtime_note':'State tests use actual source functions with mocked registry/controls; native Windows GUI and OS default changes not executed.'}
(out/'association-validation.json').write_text(json.dumps(report,indent=2));print(r.stdout);print(json.dumps(report))
