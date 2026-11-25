from pathlib import Path
import json,sys,statistics
r=Path(sys.argv[1]);p=next(r.rglob('results.json'));d=json.loads(p.read_text())
v=d['resources'];n=len(v)
print('RESULT',json.dumps({k:d[k] for k in ['commit','failure','checks','warm_p95_ms','private_peak_MiB']}))
for i in range(4):
 b=v[i*n//4:(i+1)*n//4]
 print('RESOURCE_QUARTER',i,{k:{'min':min(x[k] for x in b),'max':max(x[k] for x in b),'median':statistics.median(x[k] for x in b)} for k in ['private','gdi','user']})
print('ENDPOINTS',v[0],v[n//2],v[-1])
for row in sorted(d['samples'][100:],key=lambda x:x['ms'],reverse=True)[:20]:print('SLOW',row)
for index in range(8):
 b=[x['ms'] for x in d['samples'][100:] if x['index']==index]
 if b:print('BY_IMAGE',index,'median',statistics.median(b),'max',max(b),'count',len(b))
for p in r.rglob('fyto.ini'):
 print('FINAL_CONFIG',p.read_text(errors='replace'))
