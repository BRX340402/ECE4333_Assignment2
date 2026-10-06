"""Fit distance = K / coloured_width; input measurements CSV with width_px,distance_cm."""
import argparse,csv,statistics
p=argparse.ArgumentParser();p.add_argument('csv');a=p.parse_args()
with open(a.csv,encoding='utf-8-sig') as f:rows=list(csv.DictReader(f))
pairs=[(float(r['width_px']),float(r['distance_cm'])) for r in rows]
if not pairs or any(w<=0 or d<=0 for w,d in pairs):p.error('Need positive width_px,distance_cm rows')
k=statistics.median(w*d for w,d in pairs)
print(f'K = {k:.2f}f;')
print('Calibration-set MAE (not independent test error):',statistics.mean(abs(k/w-d) for w,d in pairs),'cm')
print('Set the corresponding *_K in Config.h. Verify at new distances, same object, camera settings and near-frontal view.')
