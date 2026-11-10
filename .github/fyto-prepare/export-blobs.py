from pathlib import Path
from hashlib import sha1
import difflib, json, base64, sys
old,new=map(Path,sys.argv[1:])
names=["voidImageViewer/src/viv.c","voidImageViewer/src/fast_jpeg.h","voidImageViewer/src/fast_png.h","voidImageViewer/src/png_decode_avx.c","voidImageViewer/vs2019/voidImageViewer.vcxproj","voidImageViewer/wuffs/wuffs-v0.3.c","voidImageViewer/wuffs/LICENSE","voidImageViewer/src/version.h","voidImageViewer/res/voidImageViewer.rc","voidImageViewer/installer/Package.wxs","voidImageViewer/installer/license.rtf","voidImageViewer/installer/setup.manifest","THIRD_PARTY_NOTICES.md","scripts/Build-Release.ps1","RELEASE_NOTES.md"]
def sha(data):
    return sha1(f'blob {len(data)}\0'.encode('ascii')+data).hexdigest()
records=[]
for path in names:
    data=(new/path).read_bytes()
    record=dict(path=path,sha=sha(data),size=len(data))
    if path.endswith('wuffs-v0.3.c'):
        record['vendor']=True
    elif len(data)>40000 and (old/path).exists():
        original=(old/path).read_bytes()
        a=original.decode('utf-8').splitlines(keepends=True)
        b=data.decode('utf-8').splitlines(keepends=True)
        record['base']=sha(original)
        record['hunks']=[[i,j,''.join(b[k:l])] for op,i,j,k,l in difflib.SequenceMatcher(None,a,b,autojunk=False).get_opcodes() if op!='equal']
    else:
        record['bytes']=base64.b64encode(data).decode('ascii')
    records.append(record)
blob=json.dumps(records,ensure_ascii=True,separators=(',',':')).encode('ascii')
for i in range(0,len(blob),7000):
    print('FYTO_SOURCE_CHUNK',i//7000,base64.b64encode(blob[i:i+7000]).decode('ascii'))
print('FYTO_SOURCE_LENGTH',len(blob))
