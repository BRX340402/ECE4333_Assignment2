"""Save the exact padded 96x96 ROI used by firmware. One labelled scene at a time."""
import argparse, io, time, urllib.request
from pathlib import Path
from PIL import Image
p=argparse.ArgumentParser()
p.add_argument('--host',default='http://192.168.4.1')
p.add_argument('--label',required=True,choices=['stop','other'])
p.add_argument('--index',type=int,default=0,help='Object index from web page; keep scene stable')
p.add_argument('--session',required=True,help='Separate session/day/location for train/test split')
p.add_argument('--count',type=int,default=50)
p.add_argument('--interval',type=float,default=1.0)
p.add_argument('--out',default='dataset')
a=p.parse_args()
if not a.session.replace('-','').replace('_','').isalnum():p.error('session: letters/numbers/-/_ only')
d=Path(a.out)/a.session/a.label;d.mkdir(parents=True,exist_ok=True)
seen=None; saved=0
try:
 while saved<a.count:
  try:
   with urllib.request.urlopen(f'{a.host}/sample?i={a.index}',timeout=15) as r:
    seq=r.headers.get('X-Frame-Seq');data=r.read()
   if seq==seen:time.sleep(a.interval);continue
   im=Image.open(io.BytesIO(data)).convert('RGB')
   if im.size!=(96,96):raise ValueError('unexpected image dimensions')
   name=f'{a.label}.{time.time_ns()}.png';im.save(d/name)
   seen=seq;saved+=1;print(f'{saved}/{a.count} {d/name}',flush=True)
  except Exception as e:print('Retry:',e,flush=True)
  time.sleep(a.interval)
except KeyboardInterrupt:pass
print('Inspect every saved image and delete wrong labels/empty crops. Do not randomly split near-duplicate frames.')
