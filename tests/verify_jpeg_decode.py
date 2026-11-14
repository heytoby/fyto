from pathlib import Path
from tempfile import TemporaryDirectory
from PIL import Image, ImageCms
import random, subprocess, sys

with TemporaryDirectory(prefix='fyto-jpeg-correctness-') as tmp:
    root=Path(tmp)
    pixels=random.Random(41).randbytes(513*347*3)
    im=Image.frombytes('RGB',(513,347),pixels)
    # A smooth region also catches color/channel shifts without noise masking them.
    for y in range(40,200):
        for x in range(40,200):im.putpixel((x,y),(x,y,175))
    paths=[]
    for label,mode,options in [
        ('baseline420','RGB',{'subsampling':2}),
        ('baseline422','RGB',{'subsampling':1}),
        ('baseline444','RGB',{'subsampling':0}),
        ('progressive','RGB',{'progressive':True}),
        ('grayscale','L',{}),
        ('icc','RGB',{'icc_profile':ImageCms.ImageCmsProfile(ImageCms.createProfile('sRGB')).tobytes()})
    ]:
        path=root/(label+'.jpg')
        im.convert(mode).save(path,quality=93,**options)
        paths.append(str(path))
    for orientation in range(2,9):
        exif=Image.Exif();exif[274]=orientation
        path=root/f'orientation-{orientation}.jpg'
        im.save(path,quality=93,exif=exif)
        paths.append(str(path))
    subprocess.run([sys.argv[1],*paths],check=True)
print('FYTO_JPEG_PIXELS_CANCELLATION_AND_TRUNCATION_VERIFIED')
