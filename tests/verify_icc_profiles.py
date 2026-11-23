"""Compare real matrix/TRC profile conversion with LittleCMS, using fixed tolerances."""
from pathlib import Path
from io import BytesIO
import os,sys,json,subprocess,hashlib,traceback
from PIL import Image,ImageCms,ImageChops,ImageStat
root=Path(__file__).resolve().parents[1]
out=root/'build/icc-profile-audit';out.mkdir(parents=True,exist_ok=True)
profiles=sorted((root/'tests/icc-profiles').glob('*.icc'))
assert len(profiles)==4,'The four pinned profile fixtures are required'
width,height=1024,768
data=bytearray(width*height*3)
for y in range(height):
    for x in range(width):
        at=(y*width+x)*3
        data[at:at+3]=bytes((16+x*223//(width-1),16+y*223//(height-1),16+(x*17+y*23)%224))
source=Image.frombytes('RGB',(width,height),bytes(data))
flags=ImageCms.Flags.NOOPTIMIZE if hasattr(ImageCms,'Flags') else ImageCms.FLAGS['NOTPRECALC']
results=[]
for profile in profiles:
    result={'profile':profile.name,'profile_sha256':hashlib.sha256(profile.read_bytes()).hexdigest()}
    try:
        image=out/(profile.stem+'.jpg');source.save(image,quality=100,subsampling=0,icc_profile=profile.read_bytes())
        decoded=Image.open(image).convert('RGB')
        reference=ImageCms.profileToProfile(decoded,ImageCms.ImageCmsProfile(BytesIO(profile.read_bytes())),
            ImageCms.createProfile('sRGB'),renderingIntent=1,outputMode='RGB',flags=flags)
        comparisons={}
        for mode in ['fast','wic']:
            path=out/(profile.stem+'-'+mode+'.raw')
            command=[sys.argv[1],str(image),str(path)]+(['wic'] if mode=='wic' else [])
            run=subprocess.run(command,capture_output=True,text=True,check=True)
            if mode=='fast':assert '(matrix)' in run.stdout,run.stdout
            raw=path.read_bytes();assert len(raw)==width*height*4
            actual=Image.frombytes('RGB',(width,height),raw,'raw','BGRX')
            difference=ImageChops.difference(actual,reference)
            mean=sum(ImageStat.Stat(difference).mean)/3
            maximum=max(channel[1] for channel in difference.getextrema())
            comparisons[mode]={'mean_channel_error':mean,'max_channel_error':maximum}
            if mode=='fast':assert mean<=.5 and maximum<=4,comparisons[mode]
        result.update(pass_=True,comparisons=comparisons)
    except BaseException:result.update(pass_=False,error=traceback.format_exc())
    results.append(result);print(json.dumps(result),flush=True)
report={'commit':os.getenv('GITHUB_SHA'),'reference':'LittleCMS relative-colorimetric, no optimization',
    'threshold':{'mean_channel_error':.5,'max_channel_error':4},'results':results}
(out/'results.json').write_text(json.dumps(report,indent=2),encoding='utf8')
if not all(x['pass_'] for x in results):raise SystemExit(1)
