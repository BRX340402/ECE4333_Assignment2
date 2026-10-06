"""Log actual device outputs to CSV via HTTP or ESP32 USB serial, no invented results."""
import argparse,csv,json,time,urllib.request
p=argparse.ArgumentParser();p.add_argument('--host',default='http://192.168.4.1')
p.add_argument('--port',help='ESP32 USB COM port, e.g. COM7; NOT UNO UART')
p.add_argument('--seconds',type=float,default=60);p.add_argument('--out',default='results.csv')
p.add_argument('--truth',default='',help='One known target: stop/other/none')
p.add_argument('--distance',type=float,default=-1,help='Measured ground truth distance cm')
a=p.parse_args();ser=None
if a.port:
 import serial
 ser=serial.Serial(a.port,115200,timeout=3)
fields=['host_time','seq','device_ms','ai','detect_enabled','color_enabled','valid','capture_ms','process_ms','inference_ms','heap_free','psram_free','line_x','line_valid','truth','true_distance_cm','label','confidence','x','y','w','h','pixels','distance_cm','uno_age_ms','uno_mode','uno_guard','uno_latched','uno_left_pwm','uno_right_pwm']
last=None;deadline=time.monotonic()+a.seconds
try:
 with open(a.out,'w',newline='',encoding='utf-8') as f:
  w=csv.DictWriter(f,fieldnames=fields);w.writeheader()
  while time.monotonic()<deadline:
   try:
    if ser:
     raw=ser.readline().decode('utf-8',errors='replace').strip()
     if not raw.startswith('{'):continue
     d=json.loads(raw)
    else:
     with urllib.request.urlopen(a.host+'/results',timeout=15) as r:d=json.load(r)
    key=(d['seq'],d['device_ms'])
    if key==last:time.sleep(.15);continue
    last=key
    base={k:d.get(k,'') for k in fields};base.update(host_time=time.time(),truth=a.truth,true_distance_cm=a.distance)
    u=d.get('uno',{})
    base.update(uno_mode=u.get('M',''),uno_guard=u.get('G',''),uno_latched=u.get('T',''),uno_left_pwm=u.get('A',''),uno_right_pwm=u.get('D',''))
    for o in d.get('objects',[]) or [{'label':'none'}]:
     row=base.copy();row.update({k:o[k] for k in fields if k in o and k!='inference_ms'})
     w.writerow(row)
    f.flush();print(d['seq'],[(o['label'],o['distance_cm']) for o in d.get('objects',[])],flush=True)
   except (ValueError,OSError,KeyError) as e:print('Read error:',e,flush=True);time.sleep(.5)
except KeyboardInterrupt:pass
finally:
 if ser:ser.close()
