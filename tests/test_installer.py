from pathlib import Path
import subprocess,ctypes as c,ctypes.wintypes as w,json,hashlib,time
root=Path.cwd();exe=root/'outputs/voidImageViewer-Setup.exe'
p=subprocess.run([str(exe),'/verify'],timeout=20);assert p.returncode==0,p.returncode
m=c.WinDLL('msi');m.MsiOpenDatabaseW.argtypes=[w.LPCWSTR,w.LPCWSTR,c.POINTER(w.UINT)];m.MsiDatabaseOpenViewW.argtypes=[w.UINT,w.LPCWSTR,c.POINTER(w.UINT)];m.MsiViewExecute.argtypes=[w.UINT,w.UINT];m.MsiViewFetch.argtypes=[w.UINT,c.POINTER(w.UINT)];m.MsiRecordGetStringW.argtypes=[w.UINT,w.UINT,w.LPWSTR,c.POINTER(w.DWORD)];m.MsiCloseHandle.argtypes=[w.UINT]
db=w.UINT();assert m.MsiOpenDatabaseW(str(root/'work/viewer-install.msi'),None,c.byref(db))==0
checks=[]
def query(sql,n):
 v=w.UINT();assert m.MsiDatabaseOpenViewW(db.value,sql,c.byref(v))==0;assert m.MsiViewExecute(v.value,0)==0
 out=[]
 while True:
  row=w.UINT();r=m.MsiViewFetch(v.value,c.byref(row))
  if r==259:break
  assert r==0
  a=[]
  for i in range(n):
   buf=c.create_unicode_buffer(2048);size=w.DWORD(2048);assert m.MsiRecordGetStringW(row.value,i+1,buf,c.byref(size))==0;a.append(buf.value)
  out.append(a);m.MsiCloseHandle(row.value)
 m.MsiCloseHandle(v.value);return out
props=query('SELECT `Property`,`Value` FROM `Property`',2);d=dict(props);assert d['ProductLanguage']=='2052';assert d['REINSTALLMODE']=='amus'
ini=query("SELECT `Attributes` FROM `Component` WHERE `Component`='DefaultIni'",1);assert int(ini[0][0])&144==144
files=query('SELECT `File`,`FileSize`,`Version` FROM `File`',3);assert next(x for x in files if x[0]=='ViewerExe')[2]=='1.0.0.16'
assert len(query('SELECT `Shortcut` FROM `Shortcut`',1))==1
assert len(query('SELECT `RemoveRegistry` FROM `RemoveRegistry`',1))==2
m.MsiCloseHandle(db.value)
report=dict(bootstrap_verify_exit=0,setup_bytes=exe.stat().st_size,product=dict((k,d[k]) for k in ['ProductName','ProductVersion','ProductCode','ProductLanguage']),preserve_config=True,old_version_registry_migration=True,full_install_test='pending',files=files)
(root/'work/installer-validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(report,ensure_ascii=True))
