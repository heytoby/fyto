from window_test_helpers import *
root=Path('work/reopen-size-check').resolve()
p=subprocess.Popen([str(root/'voidImageViewer.exe'),str(root/'photo.png')],cwd=root)
wait(lambda:any(n=='VOIDIMAGEVIEWER' for h,n in wins(p.pid)))
h=next(h for h,n in wins(p.pid) if n=='VOIDIMAGEVIEWER');time.sleep(1)
u.SetWindowPos(h,None,100,100,900,600,0x14);time.sleep(.3)
u.PostMessageW(h,0x100,27,0);p.wait(timeout=5)
results=[]
for i in range(3):
 p=subprocess.Popen([str(root/'voidImageViewer.exe'),str(root/'photo.png')],cwd=root)
 try:
  wait(lambda:any(n=='VOIDIMAGEVIEWER' for h,n in wins(p.pid)))
  h=next(h for h,n in wins(p.pid) if n=='VOIDIMAGEVIEWER');time.sleep(1)
  r=rect(h);assert (r[2]-r[0],r[3]-r[1])==(900,600),r;results.append(r)
  u.PostMessageW(h,0x100,27,0);p.wait(timeout=5)
 finally:
  if p.poll() is None:p.terminate()
print('manual resize and three reopen cycles passed',results)
Path('work/reopen-manual-results.json').write_text(json.dumps(results))
