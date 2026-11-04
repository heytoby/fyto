from pathlib import Path
import subprocess,ctypes as c,ctypes.wintypes as w,json,hashlib,time
root=Path.cwd();exe=root/'work/fyto-installer-v21/fyto-Setup.exe'
p=subprocess.run([str(exe),'/verify'],timeout=20);assert p.returncode==0,p.returncode
m=c.WinDLL('msi');m.MsiOpenDatabaseW.argtypes=[w.LPCWSTR,w.LPCWSTR,c.POINTER(w.UINT)];m.MsiDatabaseOpenViewW.argtypes=[w.UINT,w.LPCWSTR,c.POINTER(w.UINT)];m.MsiViewExecute.argtypes=[w.UINT,w.UINT];m.MsiViewFetch.argtypes=[w.UINT,c.POINTER(w.UINT)];m.MsiRecordGetStringW.argtypes=[w.UINT,w.UINT,w.LPWSTR,c.POINTER(w.DWORD)];m.MsiCloseHandle.argtypes=[w.UINT]
db=w.UINT();assert m.MsiOpenDatabaseW(str(root/'work/fyto-installer-v21/fyto.msi'),None,c.byref(db))==0
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
   buf=c.create_unicode_buffer(32768);size=w.DWORD(32768);assert m.MsiRecordGetStringW(row.value,i+1,buf,c.byref(size))==0;a.append(buf.value)
  out.append(a);m.MsiCloseHandle(row.value)
 m.MsiCloseHandle(v.value);return out

props=dict(query('SELECT `Property`,`Value` FROM `Property`',2));assert props['ProductVersion']=='1.0.21'
rtf=query("SELECT `Text` FROM `Control` WHERE `Dialog_`='LicenseAgreementDlg' AND `Control`='LicenseText'",1)[0][0]
import re
plain=re.sub(r'\\u(-?\d+)\?',lambda m:chr(int(m[1])%65536),rtf)
assert 'fyto' in plain and '基于 void Image Viewer' in plain and 'Copyright 2025 voidtools / David Carpenter' in plain
assert 'Permission is hereby granted' in plain and 'Google Inc.' in plain
binaries=query('SELECT `Name` FROM `Binary`',1);assert any('Banner' in x[0] for x in binaries);assert any('Dialog' in x[0] for x in binaries)
report={'version':props['ProductVersion'],'license_intro':'fyto Chinese intro verified in MSI Control table','upstream_copyright':'preserved','MIT_and_WebP_terms':'preserved','branded_bitmaps':'embedded','bootstrap_verify':0,'full_install':'not executed'}
(root/'work/fyto-license-validation.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))
