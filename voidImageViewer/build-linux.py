#!/usr/bin/env python3
"""Build the native Windows x64 viewer without a Windows computer.
Requires Python 3, gcc-mingw-w64-x86-64, and binutils-mingw-w64-x86-64.
"""
from pathlib import Path
import subprocess,concurrent.futures,shutil,xml.etree.ElementTree as ET
root=Path(__file__).resolve().parent;out=root/'build-linux';out.mkdir(exist_ok=True)
cc=shutil.which('x86_64-w64-mingw32-gcc') or shutil.which('x86_64-w64-mingw32-gcc-posix')
rc=shutil.which('x86_64-w64-mingw32-windres')
if not cc or not rc:raise SystemExit('Install gcc-mingw-w64-x86-64 and binutils-mingw-w64-x86-64 first.')
ns={'m':'http://schemas.microsoft.com/developer/msbuild/2003'}
project=ET.parse(root/'vs2019/voidImageViewer.vcxproj')
files=[(root/'vs2019'/x.attrib['Include'].replace('\\','/')).resolve() for x in project.findall('m:ItemGroup/m:ClCompile',ns)]
flags=['-DUNICODE','-D_UNICODE','-DWIN32','-D_WINDOWS','-DNDEBUG','-DVERSION_X64','-DBZ_NO_STDIO','-O2','-fms-extensions','-Wno-unknown-pragmas','-Wno-incompatible-pointer-types','-Wno-pointer-to-int-cast','-I'+str(root/'libwebp')]
objects=[out/(str(i)+'-'+f.stem+'.o') for i,f in enumerate(files)]
def compile_one(args):
 file,obj=args;subprocess.run([cc,*flags,'-c',str(file),'-o',str(obj)],check=True)
with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:list(pool.map(compile_one,zip(files,objects)))
support=out/'support';support.mkdir(exist_ok=True);(support/'afxres.h').write_text('#include <windows.h>\n#ifndef IDC_STATIC\n#define IDC_STATIC (-1)\n#endif\n')
resource=out/'resources.o'
subprocess.run([rc,'--preprocessor',cc,'--preprocessor-arg=-E','--preprocessor-arg=-xc','--preprocessor-arg=-DRC_INVOKED','-I'+str(support),'voidImageViewer.rc',str(resource)],cwd=root/'res',check=True)
subprocess.run([cc,'-mwindows','-static','-s','-o',str(out/'voidImageViewer.exe'),*[str(p) for p in objects],str(resource),'-lcomctl32','-lshlwapi','-lcomdlg32','-lshell32','-lole32','-loleaut32','-luuid','-luxtheme','-lgdi32','-ladvapi32'],check=True)
print(out/'voidImageViewer.exe')
