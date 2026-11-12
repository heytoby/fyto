from pathlib import Path
from hashlib import sha1
import base64,difflib,json,sys
old,new=map(Path,sys.argv[1:3])
paths=['voidImageViewer/src/viv.c','voidImageViewer/src/fast_jpeg.h','voidImageViewer/src/version.h','voidImageViewer/res/voidImageViewer.rc','voidImageViewer/installer/Package.wxs','voidImageViewer/installer/license.rtf','voidImageViewer/installer/setup.manifest','RELEASE_NOTES.md']
def blob(b):
    return sha1(f'blob {len(b)}\0'.encode()+b).hexdigest()
items=[]
for path in paths:
    before=(old/path).read_bytes()
    after=(new/path).read_bytes()
    assert before!=after,path
    row=dict(path=path,sha=blob(after),size=len(after),base=blob(before))
    if path.endswith('/viv.c'):
        a=before.decode('utf-8').splitlines(keepends=True)
        b=after.decode('utf-8').splitlines(keepends=True)
        row['hunks']=[[i,j,''.join(b[k:l])] for op,i,j,k,l in difflib.SequenceMatcher(None,a,b,autojunk=False).get_opcodes() if op!='equal']
    else:
        row['bytes']=base64.b64encode(after).decode('ascii')
    items.append(row)
data=json.dumps(items,ensure_ascii=True,separators=(',',':')).encode('ascii')
encoded=base64.b64encode(data).decode('ascii')
chunks=[encoded[i:i+3200] for i in range(0,len(encoded),3200)]
for i,chunk in enumerate(chunks):
    print(f'FYTO_PATCH {i}/{len(chunks)} {chunk}',flush=True)
