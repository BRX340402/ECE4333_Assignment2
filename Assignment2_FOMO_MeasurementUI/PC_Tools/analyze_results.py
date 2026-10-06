"""Frame-level SINGLE-TARGET evaluation, performance and distance error."""
import argparse,csv,collections,statistics,json
p=argparse.ArgumentParser();p.add_argument('csv');p.add_argument('--out',default='analysis.json');a=p.parse_args()
with open(a.csv,encoding='utf-8-sig') as f:rows=list(csv.DictReader(f))
frames=collections.defaultdict(list)
for r in rows:frames[(r['seq'],r['device_ms'])].append(r)
confusion=collections.Counter();errors=[];times=[];valid_count=0;ai_count=0
for group in frames.values():
 r=group[0];times.append(float(r['process_ms']));valid_count+=r['valid']=='1'
 if r['ai']!='1' or r['valid']!='1' or r.get('detect_enabled') not in ('1','True') or r.get('color_enabled') not in ('1','True'):continue
 ai_count+=1
 if not r['truth']:continue
 # One known target per test scene. Additional valid semantic predictions count as ambiguous, not hidden.
 candidates=[x for x in group if x['label'] in ('stop',)]
 pred=candidates[0]['label'] if len(candidates)==1 else ('ambiguous' if candidates else 'none')
 truth=r['truth'];truth='none' if truth=='other' else truth
 confusion[(truth,pred)]+=1
 if len(candidates)==1 and pred==truth:
  estimate=float(candidates[0]['distance_cm']);actual=float(r['true_distance_cm'])
  if actual>0 and estimate>=0:errors.append(abs(estimate-actual))
total=sum(confusion.values());correct=sum(v for (t,q),v in confusion.items() if t==q)
report={'frames':len(frames),'valid_frames':valid_count,'ai_valid_frames':ai_count,
 'process_ms_mean':statistics.mean(times) if times else None,
 'process_ms_max':max(times) if times else None,
 'single_target_test_frames':total,'single_target_accuracy':correct/total if total else None,
 'distance_MAE_cm':statistics.mean(errors) if errors else None,'distance_samples':len(errors),
 'confusion':{f'{t} -> {q}':v for (t,q),v in confusion.items()},
 'note':'Not mAP. Single-target scenes only. Use separate sessions; adjacent frames are correlated. No result means unmeasured.'}
for label in ['stop']:
 tp=confusion[(label,label)];fp=sum(v for (t,q),v in confusion.items() if q==label and t!=label)
 fn=sum(v for (t,q),v in confusion.items() if t==label and q!=label)
 report[label]={'precision':tp/(tp+fp) if tp+fp else None,'recall':tp/(tp+fn) if tp+fn else None}
text=json.dumps(report,indent=2,ensure_ascii=False);print(text)
with open(a.out,'w',encoding='utf-8') as f:f.write(text)
