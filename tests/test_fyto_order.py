from window_test_helpers import *
root=Path('work/fyto-order-check').resolve();root.mkdir(exist_ok=True)
shutil.copy2('work/fyto-source/voidImageViewer/vs2019/x64/Release/fyto.exe',root/'fyto.exe')
for n in ['1.png','2.png','10.png']:Image.new('RGB',(1200,800),(30,120,80)).save(root/n)
(root/'fyto.ini').write_text('[fyto]\nappdata=0\nmultiple_instances=1\nmouse_wheel_action=2\nsort_ascending=0\n')
p=subprocess.Popen([str(root/'fyto.exe'),str(root/'2.png')],cwd=root)
try:
 wait(lambda:any(n=='fyto' for h,n in wins(p.pid)));h=next(h for h,n in wins(p.pid) if n=='fyto');time.sleep(1)
 for msg,wp,name in [(0x20a,(-120&65535)<<16,'10.png'),(0x20a,120<<16,'2.png'),(0x100,37,'1.png'),(0x100,39,'2.png')]:
  u.PostMessageW(h,msg,wp,0);wait(lambda:label(h)==name);time.sleep(.2)
 print('wheel down=next, up=previous, left=previous, right=next; stale reversed settings migrated successfully')
finally:u.PostMessageW(h,0x10,0,0);p.wait(timeout=5)
