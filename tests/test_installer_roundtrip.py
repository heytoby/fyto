from pathlib import Path
import subprocess,hashlib,json,shutil
root=Path.cwd();target=root/'work/installer-sandbox/app';shortcuts=root/'work/installer-sandbox/shortcuts';target.mkdir(parents=True,exist_ok=True);shortcuts.mkdir(parents=True,exist_ok=True)
msi=root/'work/viewer-install-test.msi';results=[]
def run(stage,*extra):
 log=root/f'work/installer-{stage}.log';p=subprocess.run([r'C:\Windows\System32\msiexec.exe',*extra,'/qn','/norestart','/L*v',str(log)],timeout=50)
 results.append(dict(stage=stage,exit=p.returncode));print(stage,p.returncode,flush=True);return p.returncode
installed=False
try:
 r=run('fresh','/i',str(msi),'INSTALLFOLDER='+str(target),'ProgramMenuFolder='+str(shortcuts))
 assert r in (0,3010),r;installed=True
 expected=(root/'outputs/voidImageViewer-ready/voidImageViewer.exe').read_bytes();assert (target/'voidImageViewer.exe').read_bytes()==expected
 ini=b'[voidImageViewer]\r\nappdata=0\r\nlanguage=0\r\ncustom_keep=1\r\n';(target/'voidImageViewer.ini').write_bytes(ini);(target/'user-photo.txt').write_text('preserve user files')
 shutil.copy2(root/'outputs/voidImageViewer-ready/README.txt',target/'voidImageViewer.exe')
 r=run('overwrite','/i',str(msi),'REINSTALL=ALL','REINSTALLMODE=amus','INSTALLFOLDER='+str(target),'ProgramMenuFolder='+str(shortcuts));assert r in (0,3010),r
 assert (target/'voidImageViewer.exe').read_bytes()==expected
 assert (target/'voidImageViewer.ini').read_bytes()==ini
 assert (target/'user-photo.txt').read_text()=='preserve user files'
 assert list(shortcuts.glob('*.lnk')),'missing shortcut'
 results.append(dict(stage='overwrite-verification',executable_restored=True,config_preserved=True,user_file_preserved=True,shortcut=True))
finally:
 if installed:
  r=run('uninstall','/x',str(msi));assert r in (0,3010),r
  assert not (target/'voidImageViewer.exe').exists();assert (target/'voidImageViewer.ini').exists();assert (target/'user-photo.txt').exists()
 (root/'work/installer-roundtrip.json').write_text(json.dumps(results,indent=2))
print('install/overwrite/uninstall passed')
