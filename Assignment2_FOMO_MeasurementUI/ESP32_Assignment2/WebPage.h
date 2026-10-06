#pragma once
static const char PAGE[] PROGMEM=R"HTML(<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>ECE4333 Robot Control</title>
<style>*{box-sizing:border-box}body{font:15px system-ui;background:#edf2f8;color:#14233b;margin:0;padding:20px}main{max-width:1100px;margin:auto}.grid{display:grid;grid-template-columns:1.25fr 1fr;gap:18px}.panel{background:white;padding:20px;border-radius:15px;margin-bottom:16px}canvas{width:100%;background:#14233b;border-radius:10px}h1{margin:0}h2{font-size:19px;margin-top:0}button,select,input{font:inherit}button{border:0;border-radius:8px;padding:12px;background:#e4ebf5;color:#14233b;cursor:pointer;margin:3px;touch-action:none}button:active{background:#96b9f0}button:disabled{opacity:.5}.stop{background:#cc2035;color:white}.directions{display:grid;grid-template-columns:repeat(3,1fr);max-width:300px;margin:10px auto}.directions button{height:55px;margin:3px}.line{display:flex;gap:8px;align-items:center;flex-wrap:wrap}input[type=range]{flex:1}#notice{padding:10px;background:#fff4d6;border-radius:8px}#status{font-size:14px;white-space:pre-wrap}pre{white-space:pre-wrap;max-height:240px;overflow:auto;font-size:12px}.pill{padding:5px 9px;border-radius:6px;background:#e4ebf5}a{color:#245dad}.small{font-size:13px;color:#58667a}@media(max-width:760px){.grid{grid-template-columns:1fr}body{padding:10px}}table{width:100%;border-collapse:collapse;font-size:13px}td,th{text-align:left;padding:8px;border-bottom:1px solid #ddd}.scroll{overflow:auto}#calNote{white-space:pre-wrap}</style>
<main><h1>ECE4333 · Robot Control</h1><p>ESP32 vision + UNO motors & sensors</p><p id="notice">Connecting. Vehicle starts in standby.</p>
<div class="grid"><div><section class="panel"><div class="line"><h2>Camera & recognition</h2><span id="model" class="pill">No model</span></div><canvas id="view" width="320" height="240"></canvas>
<p><button onclick="preview=!preview;this.textContent=preview?'Pause preview':'Start preview'">Pause preview</button><button onclick="downloadFrame()">Save full photo</button><a href="http://192.168.4.1:81/stream" target="_blank">MJPEG stream</a></p>
<div class="line"><label><input id="color" type="checkbox" checked onchange="send({action:'color',value:+this.checked})"> Colour regions</label><label><input id="detect" type="checkbox" onchange="send({action:'detect',value:+this.checked})"> AI stop detection</label></div>
<p class="small">Class confidence is not test accuracy. Distance −1 means uncalibrated or invalid.</p>
<div class="line"><label>Object index <select id="object"></select></label><button onclick="saveCrop()">Save object preview (not FOMO training data)</button></div><div class="scroll"><table><thead><tr><th>Object / score</th><th>Centre (px)</th><th>Size (px)</th><th>Distance</th></tr></thead><tbody id="objects"></tbody></table></div>
<p class="small">320 × 240 image; origin at top left. Size is the coloured region, not physical centimetres. FOMO alone gives location, not sign size. Colour regions are not AI object classes.</p>
<h2>Distance calibration</h2>
<p class="small">Stop the car. Face one complete object toward the camera. Select its index above, then sample its measured width. Measure from the camera lens to the object plane. One calibration per object type and physical size.</p>
<button onclick="sampleMeasurement()">Sample selected object width</button>
<div id="calNote">No width sampled.</div>
<div class="line"><label>Known distance (cm) <input id="knownCm" type="number" min="5" max="1000" step="1" value="50" style="width:85px"></label><button onclick="saveCalibration(false)">Save calibration</button><button onclick="saveCalibration(true)">Clear selected calibration</button></div>
<p class="small">Distance ≈ K / width(px). Saved on ESP32 across restarts. Recalibrate for a different sign size. Verify at two other measured distances. Calibration stops driving; resume manually.</p><div id="calValues" class="small"></div></section>
<section class="panel"><h2>UNO sensors & state</h2><div id="status">Waiting for UNO…</div><details><summary>Raw recognition output</summary><pre id="raw"></pre></details></section></div>
<div><section class="panel"><h2>Manual drive</h2><div class="line"><span>PWM speed</span><input id="speed" type="range" min="0" max="180" value="90" oninput="speedValue.textContent=this.value" onchange="send({action:'speed',value:this.value})"><output id="speedValue">90</output></div>
<div class="directions"><button data-dir="5">↖</button><button data-dir="1">↑</button><button data-dir="7">↗</button><button data-dir="3">↶</button><button class="stop" onclick="stopAll()">STOP</button><button data-dir="4">↷</button><button data-dir="6">↙</button><button data-dir="2">↓</button><button data-dir="8">↘</button></div>
<p class="small">Hold to drive, release to stop. Arrow keys supported; Space stops. Physical power switch removes power immediately.</p>
<h2>Driving modes</h2><div class="line"><button onclick="mode(0)">Standby</button><button onclick="mode(1)">IR line tracking</button><button onclick="mode(2)">Avoid obstacles</button><button onclick="mode(3)">Range follow</button><button id="aiMode" onclick="mode(4)">AI stop + IR line</button></div>
<p class="small">IR line tracking uses the three floor sensors, not camera pixels. Range follow uses ultrasound, not face tracking.</p>
<div class="line"><label><input id="guard" type="checkbox" onchange="held=0;active=false;send({action:'guard',value:+this.checked})"> Apply AI stop guard to movement</label><button onclick="held=0;active=false;send({action:'reset_stop'})">Reset stop latch</button></div></section>
<section class="panel"><h2>Camera / ultrasonic pan</h2><div class="line"><button onclick="panBy(15)">Left 15°</button><button onclick="setPan(90)">Centre 90°</button><button onclick="panBy(-15)">Right 15°</button></div><div class="line"><input id="pan" type="range" min="0" max="180" value="90" oninput="panValue.textContent=this.value" onchange="setPan(+this.value)"><output id="panValue">90</output></div><p class="small">Pan commands first stop driving. Range 0–180°; check physical clearance.</p>
<h2>RGB status light</h2><input id="rgb" type="color" value="#0088ff"><button onclick="setLED()">Set colour</button><button onclick="send({action:'led',r:0,g:0,b:0})">Light off</button></section></div></div></main>
<script>
const el=s=>document.getElementById(s),cv=el('view'),ctx=cv.getContext('2d');let preview=true,active=false,held=0,movePending=false,lastData=null;
let commandChain=Promise.resolve();
function send(data){const run=()=>sendNow(data);commandChain=commandChain.then(run,run);return commandChain;}
async function sendNow(data){try{let r=await fetch('/cmd',{method:'POST',body:new URLSearchParams(data)});let t=await r.text();if(!r.ok)throw Error(t);el('notice').textContent='Command sent; verify UNO telemetry. '+t;return true;}catch(e){el('notice').textContent=String(e);return false;}}
function stopAll(){held=0;active=false;send({action:'stop'});}
async function mode(n){held=0;active=n!==0;if(!await send({action:'mode',value:n}))active=false;}
function setPan(n){stopAll();el('pan').value=n;el('panValue').textContent=n;send({action:'servo',value:n});}
function panBy(n){setPan(Math.max(0,Math.min(180,+el('pan').value+n)));}
function setLED(){let c=el('rgb').value;send({action:'led',r:parseInt(c.slice(1,3),16),g:parseInt(c.slice(3,5),16),b:parseInt(c.slice(5,7),16)});}
async function drive(){if(!held||movePending)return;movePending=true;try{if(!await send({action:'move',dir:held,speed:el('speed').value}))stopAll();}finally{movePending=false;}}
for(let b of document.querySelectorAll('[data-dir]')){b.addEventListener('pointerdown',e=>{e.preventDefault();b.setPointerCapture(e.pointerId);held=+b.dataset.dir;active=true;drive();});b.addEventListener('pointerup',stopAll);b.addEventListener('pointercancel',stopAll);b.addEventListener('lostpointercapture',()=>{if(held)stopAll();});}
const keys={ArrowUp:1,ArrowDown:2,ArrowLeft:3,ArrowRight:4};window.addEventListener('keydown',e=>{if(/INPUT|SELECT/.test(e.target.tagName))return;if(e.code==='Space'){e.preventDefault();stopAll();}else if(keys[e.code]&&!e.repeat){e.preventDefault();held=keys[e.code];active=true;drive();}});window.addEventListener('keyup',e=>{if(keys[e.code])stopAll();});window.addEventListener('blur',stopAll);document.addEventListener('visibilitychange',()=>{if(document.hidden)stopAll();});window.addEventListener('pagehide',()=>navigator.sendBeacon('/cmd',new URLSearchParams({action:'stop'})));
setInterval(()=>{if(held)drive();else if(active)fetch('/cmd',{method:'POST',body:new URLSearchParams({action:'ping'})}).catch(()=>{});},250);
function download(blob,name){let u=URL.createObjectURL(blob),a=document.createElement('a');a.href=u;a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(u),2000);}
async function saveCrop(){try{let r=await fetch('/sample?i='+el('object').value);if(!r.ok)throw Error(await r.text());download(await r.blob(),'label_me_'+Date.now()+'.bmp');}catch(e){el('notice').textContent=String(e);}}
async function downloadFrame(){try{let r=await fetch('/capture');if(!r.ok)throw Error(await r.text());download(await r.blob(),'scene_'+Date.now()+'.jpg');}catch(e){el('notice').textContent=String(e);}}
const modes=['Standby','IR line','Obstacle avoidance','Range follow','AI + IR line','Manual'];const reasons=['Standby','Running','Remote timeout','AI missing/stale','Line lost','STOP latched','','','No range / out of follow range'];
let calibrationSample=null;
const distanceReasons={not_calibrated:'Not calibrated',no_matched_red_region:'No sign size',clipped_or_too_small:'Clipped / too small',out_of_range:'Out of range'};
function distanceText(o){return o.distance_valid?'~'+o.distance_cm+' cm':(distanceReasons[o.distance_reason]||'Unavailable');}
function renderMeasurements(d){
 const tbody=el('objects');tbody.replaceChildren();
 for(const o of d.objects||[]){const tr=document.createElement('tr');
  const values=[o.label+(o.id===1?' '+(o.confidence*100).toFixed(1)+'%':''),`(${o.cx}, ${o.cy})`,o.size_source==='fomo_location_only'?'Location only':`${o.w} × ${o.h}`,distanceText(o)];
  for(const value of values){const td=document.createElement('td');td.textContent=value;tr.appendChild(td);}tbody.appendChild(tr);
 }
 if(!(d.objects||[]).length){const tr=document.createElement('tr'),td=document.createElement('td');td.colSpan=4;td.textContent='No detected objects';tr.appendChild(td);tbody.appendChild(tr);}
 el('calValues').textContent='Saved K (cm·px): '+['stop','red','blue','green','orange'].map((n,i)=>n+' '+((d.calibration_k||[])[i]||0)).join(' / ');
}
function sampleMeasurement(){
 stopAll();const d=lastData,o=d&&(d.objects||[]).find(o=>o.index===+el('object').value);
 if(!d||!d.valid||d.frame_age_ms>2000||Date.now()-lastReceivedAt>2500||!o||!o.size_valid){calibrationSample=null;el('calNote').textContent='Need a fresh, complete measured region. For STOP, keep the red sign fully visible.';return;}
 calibrationSample={id:o.id,width:o.w,seq:d.seq};
 el('calNote').textContent=`Sample: ${o.label}, width ${o.w}px, frame ${d.seq}. Keep the object at this measured distance before saving.`;
}
async function saveCalibration(clear){
 const o=lastData&&(lastData.objects||[]).find(o=>o.index===+el('object').value);
 if((!clear&&!calibrationSample)||(clear&&!o)){el('calNote').textContent='Sample a valid object first, or select an object to clear.';return;}
 const cm=Number(el('knownCm').value);if(!clear&&(!Number.isInteger(cm)||cm<5||cm>1000)){el('calNote').textContent='Enter a whole number from 5 to 1000 cm.';return;}
 stopAll();const data=clear?{id:o.id,clear:1}:{...calibrationSample,cm};
 try{await commandChain;const r=await fetch('/calibrate',{method:'POST',body:new URLSearchParams(data)});const t=await r.text();if(!r.ok)throw Error(t);el('calNote').textContent=t;calibrationSample=null;}catch(e){el('calNote').textContent=String(e);}
}
let lastReceivedAt=0;
async function poll(){try{let r=await fetch('/results');let d=await r.json();lastData=d;lastReceivedAt=Date.now();el('model').textContent=d.ai?(d.ai_error?'AI error '+d.ai_error:(d.detect_enabled?'AI active':'AI off')):'Colour only';el('detect').disabled=!d.ai;el('aiMode').disabled=!d.ai;el('detect').checked=!!d.detect_enabled;el('color').checked=!!d.color_enabled;
 renderMeasurements(d);
 let old=el('object').value;el('object').replaceChildren(...(d.objects||[]).map(o=>{let v=document.createElement('option');v.value=o.index;v.textContent='#'+o.index+' '+o.label;return v;}));if([...el('object').options].some(o=>o.value===old))el('object').value=old;
 let u=d.uno||{};el('guard').checked=!!u.G;el('status').textContent=u.N===210?`Mode: ${modes[u.M]}\nState: ${reasons[u.B]||u.B}; stop latch: ${u.T}\nUltrasonic: ${u.U}cm; battery estimate: ${(u.V/1000).toFixed(2)}V\nFloor L/M/R: ${u.L} / ${u.C} / ${u.R}\nMotor L/R: ${u.A} / ${u.D}; pan: ${u.S}°\nUNO data age: ${d.uno_age_ms}ms`:'No UNO response: check upload, Cam switch and wiring';el('raw').textContent=JSON.stringify(d,null,2);
 if(preview){let imgr=await fetch('/capture');if(imgr.ok){let im=await createImageBitmap(await imgr.blob());ctx.drawImage(im,0,0);im.close();for(let o of (Number(imgr.headers.get('X-Frame-Seq'))===d.seq?d.objects||[]:[])){ctx.strokeStyle=o.id===1?'#ff3355':['#f44','#49f','#1e6','#fa2'][o.color];ctx.strokeRect(o.x,o.y,o.w,o.h);ctx.fillStyle=ctx.strokeStyle;ctx.fillText('#'+o.index+' '+o.label+(o.distance_valid?' ~'+o.distance_cm+'cm':''),o.x,Math.max(10,o.y-3));ctx.beginPath();ctx.moveTo(o.cx-4,o.cy);ctx.lineTo(o.cx+4,o.cy);ctx.moveTo(o.cx,o.cy-4);ctx.lineTo(o.cx,o.cy+4);ctx.stroke();}}}
}catch(e){el('status').textContent='Reconnecting: '+e;}setTimeout(poll,600);}poll();
</script></html>)HTML";
