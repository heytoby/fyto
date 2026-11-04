from window_test_helpers import *
import sys
root=Path('work/reopen-size-check').resolve();root.mkdir(exist_ok=True)
shutil.copy2(sys.argv[1],root/'voidImageViewer.exe')
Image.new('RGB',(1200,800),(30,120,80)).save(root/'photo.png')
Image.new('RGB',(800,1200),(30,120,80)).save(root/'portrait.png')
(root/'voidImageViewer.ini').write_text('[voidImageViewer]\nappdata=0\nmultiple_instances=1\nwide=800\nhigh=600\n',encoding='ascii')
sizes=[]
for i in range(6):
 p=subprocess.Popen([str(root/'voidImageViewer.exe'),str(root/('portrait.png' if '--alternate' in sys.argv and i%2 else 'photo.png'))],cwd=root)
 try:
  wait(lambda:any(n=='VOIDIMAGEVIEWER' for h,n in wins(p.pid)))
  h=next(h for h,n in wins(p.pid) if n=='VOIDIMAGEVIEWER')
  wait(lambda:label(h).endswith('.png'));time.sleep(.7)
  r=rect(h);sizes.append([r[2]-r[0],r[3]-r[1]])
  u.PostMessageW(h,0x100,27,0);p.wait(timeout=5)
 finally:
  if p.poll() is None:p.terminate()
print(json.dumps(sizes),flush=True)
Path('work/reopen-size-results.json').write_text(json.dumps(sizes))
if '--assert' in sys.argv:assert all(s==sizes[i%2 if '--alternate' in sys.argv else 0] for i,s in enumerate(sizes)),sizes
