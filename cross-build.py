from pathlib import Path
import subprocess,concurrent.futures,os,xml.etree.ElementTree as ET,json
import argparse
parser=argparse.ArgumentParser();parser.add_argument('--toolchain',required=True,help='MinGW usr prefix');parser.add_argument('--output',default='cross-build');args=parser.parse_args()
root=Path(__file__).resolve().parent;src=root/'voidImageViewer';build=Path(args.output).resolve();build.mkdir(exist_ok=True,parents=True);tc=Path(args.toolchain).resolve();cc=str(tc/'bin/x86_64-w64-mingw32-gcc-posix')
env=os.environ.copy();env['PATH']=str(tc/'bin')+':'+env['PATH'];env['LD_LIBRARY_PATH']=str(tc/'lib/x86_64-linux-gnu')
flags=['-DUNICODE','-D_UNICODE','-DWIN32','-D_WINDOWS','-DNDEBUG','-DVERSION_X64','-DBZ_NO_STDIO','-O2','-fms-extensions','-Wno-unknown-pragmas','-Wno-incompatible-pointer-types','-Wno-pointer-to-int-cast','-I'+str(src/'libwebp')]
proj=ET.parse(src/'vs2019/voidImageViewer.vcxproj');ns={'ms':'http://schemas.microsoft.com/developer/msbuild/2003'}
files=[(src/'vs2019'/x.attrib['Include'].replace('\\','/')).resolve() for x in proj.findall('.//ms:ClCompile[@Include]',ns)]
files=list(dict.fromkeys(files));(build/'sources.json').write_text(json.dumps([str(p) for p in files],indent=2))
def compile_file(item):
 i,p=item;out=build/f'{i}-{p.stem}.o';compiler=cc if p.suffix=='.c' else str(tc/'bin/x86_64-w64-mingw32-g++-posix');r=subprocess.run([compiler,*flags,'-c',str(p),'-o',str(out)],capture_output=True,text=True,env=env);return p,r.returncode,r.stderr,str(out)
results=[(p,0,'',str(build/f'{i}-{p.stem}.o')) for i,p in enumerate(files)] if os.environ.get('FYTO_LINK_ONLY') else list(concurrent.futures.ThreadPoolExecutor(max_workers=6).map(compile_file,enumerate(files)))
(build/'compiler.log').write_text('\n'.join(str(p)+'\n'+err for p,c,err,o in results if err))
for p,c,err,o in results:
 if c:print(str(p)+'\n'+err[-5000:])
print('compiled',sum(not r[1] for r in results),'/',len(results),flush=True)
if any(r[1] for r in results):raise SystemExit(1)
rc=src/'res/voidImageViewer.rc';res=build/'viewer-resource.o';portable_rc=build/'viewer-portable.rc';portable_rc.write_bytes(rc.read_bytes().replace(b'..\\\\LICENSE',b'../LICENSE'))
r=subprocess.run([str(tc/'bin/x86_64-w64-mingw32-windres'),'--preprocessor='+cc,'--preprocessor-arg=-E','--preprocessor-arg=-xc','--preprocessor-arg=-DRC_INVOKED','-I',str(root/'build-support'),'-I',str(rc.parent),str(portable_rc),str(res)],cwd=rc.parent,env=env,capture_output=True,text=True)
print(r.stderr);assert r.returncode==0
cmd=[cc,'-static','-static-libgcc','-mwindows','-Wl,--nxcompat,--dynamicbase','-o',str(build/'fyto.exe'),*[o for p,c,e,o in results],str(res),'-lcomctl32','-lshlwapi','-lmsimg32','-limm32','-lurlmon','-lole32','-luuid','-lcomdlg32','-lversion','-ladvapi32','-lshell32','-lgdi32','-luser32','-lwinmm','-lws2_32','-luxtheme','-ldwrite','-ld2d1','-lstdc++']
r=subprocess.run(cmd,env=env,capture_output=True,text=True);(build/'link.log').write_text(r.stdout+r.stderr);print(r.stderr[-5000:]);assert r.returncode==0
subprocess.run([str(tc/'bin/x86_64-w64-mingw32-strip'),str(build/'fyto.exe')],check=True,env=env)
print('viewer built', (build/'fyto.exe').stat().st_size)
