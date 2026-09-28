#include <Arduino.h>
#include <atomic>
#include <Preferences.h>
#include <cmath>
#include "Config.h"
#include <WiFi.h>
#include <WebServer.h>
#include "esp_camera.h"
#include "img_converters.h"
#include "camera_pins.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "StreamServer.h"
#include "Preview.h"
#include <esp_heap_caps.h>
#include "CameraDecode.h"
#include "FaceAdapter.h"
#include "AIAdapter.h"
#include "WebPage.h"
WebServer web(80);
int analysisW=320,analysisH=240;
bool inputBGR=true;
ViewMap aiView=makeView(320,240,1,500,500);
int viewMode=1,viewCx=500,viewCy=500;
float aiThreshold=0.80f,bestScore=0;
int stopCount=0,photoW=320,photoH=240;
FaceOutput faces={};
uint8_t *completedJpeg=nullptr;size_t completedLen=0;
uint8_t inputBMP[54+96*96*3];bool inputReady=false;
int calibrationResolution=4;
uint16_t geometryEpoch=0;
String cameraStatus="{}";

Preferences calibration;
bool calibrationReady=false;
float distanceK[5]={STOP_K,0,0,0,0}; // stop, red, blue, green, orange
int calibrationSlot(int id){return id==1?0:(id>=6&&id<=9?id-5:-1);}
const char *calibrationKeys[5]={"stop","red","blue","green","orange"};
bool usableSize(const ColorBlob &b){return b.pixels && b.x1-b.x0+1>=6 && b.x0>0 && b.y0>0 && b.x1<analysisW-1 && b.y1<analysisH-1;}

struct Detection { ColorBlob box;int color,id,distance;float confidence;uint32_t inference_ms;int aiX,aiY; } detections[8];
uint8_t *rgb=nullptr,*mask=nullptr;uint32_t *queueBuffer=nullptr;
camera_fb_t *frame=nullptr;
uint8_t objectCount=0;uint16_t sequence=0;
bool cameraOK=false,rgbOK=false;int aiError=0;
bool frameValid=false;int lineX=-1;bool lineValid=false;
uint32_t nextFrame=0,captureMs=0,processMs=0,totalInferenceMs=0,frameTime=0;
SemaphoreHandle_t frameMutex,stateMutex;
QueueHandle_t visionQueue;
String cachedResults="{\"seq\":0,\"ai\":0,\"objects\":[]}",unoStatus="{}",lastReply="";
uint32_t lastUno=0;
std::atomic<bool> detectEnabled{true},colorEnabled{true};
WiFiServer appServer(100);WiFiClient appClient;
String appRx,unoRx;
uint32_t appLastSeen=0,appHeartbeat=0;
int owner=0; // 0 none/local, 1 browser, 2 official APP
struct VisionPayload { uint16_t seq;bool ai,valid;int confidence,distance,cx,cy,w,h;uint32_t captured; };
void sendJSON(const String &s){Serial2.println(s);}
int distanceFor(int id,const ColorBlob &b){
  int slot=calibrationSlot(id);
  float k=slot>=0?distanceK[slot]:0;
  int w=b.x1-b.x0+1;
  if(k<=0 || !usableSize(b))return -1;
  float d=k*analysisW/w;if(!std::isfinite(d)||d>10000)return -1;return (int)(d+0.5f);
}
void publish(){
  VisionPayload v={sequence,(bool)A2_ENABLE_AI,frameValid,0,-1,-1,-1,0,0,frameTime};
  if(!detectEnabled)v.ai=false;
  for(int i=0;i<objectCount;i++){
    Detection &d=detections[i];
    if(d.id==1 && (v.confidence==0 || ((d.confidence>=0.8f)==(v.confidence>=800) ? (d.distance<0 || (v.distance>=0 && d.distance<v.distance)) : d.confidence>=0.8f))){
      v.confidence=(int)(d.confidence*1000);v.distance=d.distance;
      v.cx=d.box.x;v.cy=d.box.y;v.w=d.box.x1-d.box.x0+1;v.h=d.box.y1-d.box.y0+1;
    }
  }
  xQueueOverwrite(visionQueue,&v);
}
String results(){
  String s;s.reserve(3500);
  s="{\"seq\":"+String(sequence)+",\"device_ms\":"+String(frameTime)+",\"ai\":"+String(A2_ENABLE_AI)+",\"valid\":"+String(frameValid);
  s+=",\"frame_age_ms\":"+String(millis()-frameTime)+",\"capture_ms\":"+String(captureMs)+",\"process_ms\":"+String(processMs);
  s+=",\"inference_ms\":"+String(totalInferenceMs)+",\"heap_free\":"+String(ESP.getFreeHeap())+",\"psram_free\":"+String(ESP.getFreePsram());
  s+=",\"line_x\":"+String(lineX)+",\"line_valid\":"+String(lineValid)+",\"objects\":[";
  for(int i=0;i<objectCount;i++){
    Detection &d=detections[i];ColorBlob &b=d.box;if(i)s+=',';
    s+="{\"index\":"+String(i)+",\"color\":"+String(d.color)+",\"id\":"+String(d.id)+",\"label\":\""+className(d.id)+"\",\"confidence\":"+String(d.confidence,4);
    s+=",\"cx\":"+String(b.x)+",\"cy\":"+String(b.y)+",\"x\":"+String(b.x0)+",\"y\":"+String(b.y0)+",\"w\":"+String(b.x1-b.x0+1)+",\"h\":"+String(b.y1-b.y0+1);
    bool measured=b.pixels>0;int slot=calibrationSlot(d.id);
    const char *reason=!measured?"no_matched_red_region":(!usableSize(b)?"clipped_or_too_small":(slot<0||distanceK[slot]<=0?"not_calibrated":(d.distance<0?"out_of_range":"ok")));
    s+=",\"ai_cx\":"+String(d.aiX)+",\"ai_cy\":"+String(d.aiY);
    s+=",\"size_source\":\""+String(d.id==1?(measured?"matched_red_region":"fomo_location_only"):"colour_region")+"\",\"size_valid\":"+String(usableSize(b));
    s+=",\"distance_valid\":"+String(d.distance>=0)+",\"distance_reason\":\""+reason+"\"";
    s+=",\"pixels\":"+String(b.pixels)+",\"distance_cm\":"+String(d.distance)+",\"inference_ms\":"+String(d.inference_ms)+"}";
  }
  s+="],\"camera_ok\":"+String(cameraOK)+",\"rgb_ok\":"+String(rgbOK)+",\"ai_error\":"+String(aiError);
  s+=",\"heap_largest\":"+String(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
  s+=",\"image_w\":"+String(analysisW)+",\"image_h\":"+String(analysisH)+",\"photo_w\":"+String(photoW)+",\"photo_h\":"+String(photoH);
  s+=",\"best_score\":"+String(bestScore,4)+",\"threshold\":"+String(aiThreshold,2)+",\"model_min_score\":0.5,\"stop_count\":"+String(stopCount);
  s+=",\"roi\":{\"x\":"+String(aiView.x)+",\"y\":"+String(aiView.y)+",\"w\":"+String(aiView.w)+",\"h\":"+String(aiView.h)+"}";
  s+=",\"face_count\":"+String(faces.count)+",\"face_id\":"+String(faces.id)+",\"face_error\":"+String(faces.error)+",\"face_samples_left\":"+String(faces.samples)+",\"faces\":[";
  for(int i=0;i<faces.count;i++){if(i)s+=',';FaceBox &b=faces.boxes[i];s+="{\"x\":"+String(b.x)+",\"y\":"+String(b.y)+",\"w\":"+String(b.w)+",\"h\":"+String(b.h)+"}";}s+="]";
  s+=",\"framesize\":"+String((int)esp_camera_sensor_get()->status.framesize)+",\"epoch\":"+String(geometryEpoch)+",\"calibration_k\":[";
  for(int i=0;i<5;i++){if(i)s+=',';s+=String(distanceK[i],2);}
  return s+"]}";
}
void detectLine(){
  uint32_t sum=0,count=0;const int y0=analysisH*3/4;
  for(int y=y0;y<analysisH;y++)for(int x=0;x<analysisW;x++){
    const uint8_t*p=rgb+3*(y*analysisW+x);
    if(p[0]<BLACK_MAX&&p[1]<BLACK_MAX&&p[2]<BLACK_MAX){sum+=x;count++;}
  }
  // Reject almost empty and mostly dark floor regions. Not an intersection navigator.
  lineValid=count>=100&&count<(uint32_t)(analysisW*(analysisH-y0)/3);
  lineX=lineValid?sum/count:-1;
}
bool plausibleSign(const ColorBlob &b){
 int w=b.x1-b.x0+1,h=b.y1-b.y0+1;
 return usableSize(b) && w>=12 && h>=12 && w*100>=h*65 && w*100<=h*150 && b.pixels*100u>=(uint32_t)(w*h*20);
}
void makeModelBMP();
void acquire(){
 uint32_t start=millis();frameValid=false;objectCount=0;lineValid=false;lineX=-1;sequence++;
 totalInferenceMs=0;aiError=0;cameraOK=false;rgbOK=false;bestScore=0;stopCount=0;faces={};faces.id=-2;
 if(frame){esp_camera_fb_return(frame);frame=nullptr;}
 frame=esp_camera_fb_get();frameTime=millis();captureMs=millis()-start;
 cameraOK=frame && frame->format==PIXFORMAT_JPEG && frame->len>0;
 if(cameraOK){previewStore(frame,sequence);photoW=frame->width;photoH=frame->height;}
 if(!cameraOK || !decodeBounded(frame,rgb,analysisW,analysisH)){inputReady=false;processMs=millis()-start;publish();return;}
 rgbOK=true;frameValid=true;detectLine();aiView=makeView(analysisW,analysisH,viewMode,viewCx,viewCy);aiPixels=rgb;makeModelBMP();
 StopPrediction stop={};
 if(detectEnabled){if(!inferFrame(rgb,stop))frameValid=false;totalInferenceMs=stop.duration;aiError=stop.error;bestScore=stop.best;}
 stopCount=stop.count;
 for(int i=0;i<stop.count;i++){
  Detection &d=detections[objectCount++];d.box=stop.box[i];d.color=0;d.id=1;d.confidence=stop.confidence[i];d.inference_ms=stop.duration;d.distance=-1;d.aiX=stop.box[i].x;d.aiY=stop.box[i].y;
  ColorBlob red=find_color_blob(rgb,analysisW,analysisH,mask,queueBuffer,TARGET_RED,d.aiX,d.aiY);
  if(plausibleSign(red)){d.box=red;d.distance=distanceFor(1,red);}
 }
 for(int color=0;color<4 && colorEnabled;color++){
  ColorBlob b=find_color_blob(rgb,analysisW,analysisH,mask,queueBuffer,(TargetColor)color);if(!b.pixels)continue;
  bool duplicate=false;for(int i=0;i<stop.count;i++)if(color==0 && detections[i].box.pixels && b.x0==detections[i].box.x0 && b.x1==detections[i].box.x1 && b.y0==detections[i].box.y0 && b.y1==detections[i].box.y1)duplicate=true;
  if(duplicate||objectCount>=8)continue;
  Detection &d=detections[objectCount++];d.box=b;d.color=color;d.id=color+6;d.confidence=0;d.inference_ms=0;d.distance=distanceFor(d.id,b);d.aiX=-1;d.aiY=-1;
 }
 if(faceEnabled)faces=processFaces(rgb,analysisW,analysisH);
 processMs=millis()-start;publish();
}

void write32(uint8_t*p,uint32_t x){for(int i=0;i<4;i++)p[i]=(x>>(8*i))&255;}
void makeModelBMP(){
 memset(inputBMP,0,54);inputBMP[0]='B';inputBMP[1]='M';write32(inputBMP+2,sizeof(inputBMP));write32(inputBMP+10,54);write32(inputBMP+14,40);write32(inputBMP+18,96);write32(inputBMP+22,96);inputBMP[26]=1;inputBMP[28]=24;
 for(int y=0;y<96;y++)for(int x=0;x<96;x++){int sx,sy;uint32_t c=viewPixel(aiView,x,y,sx,sy)?packedPixel(rgb+3*(sy*analysisW+sx)):0;int k=54+3*((95-y)*96+x);inputBMP[k]=c&255;inputBMP[k+1]=(c>>8)&255;inputBMP[k+2]=(c>>16)&255;}inputReady=true;
}
void sample(){
  if(!frameValid||!web.hasArg("i")){web.send(400,"text/plain","Need a valid frame and i=object index");return;}
  int i=web.arg("i").toInt();if(i<0||i>=objectCount){web.send(404,"text/plain","No such object");return;}
  uint8_t *bmp=(uint8_t*)ps_malloc(54+96*96*3);
  if(!bmp){web.send(503,"text/plain","No memory");return;}
  memset(bmp,0,54);bmp[0]='B';bmp[1]='M';write32(bmp+2,54+96*96*3);write32(bmp+10,54);write32(bmp+14,40);
  write32(bmp+18,96);write32(bmp+22,96);bmp[26]=1;bmp[28]=24;
  Crop crop=cropFor(detections[i].box);
  for(int y=0;y<96;y++)for(int x=0;x<96;x++){
    uint32_t p=cropPixel(rgb,crop,x,y);size_t k=54+3*((95-y)*96+x);
    bmp[k]=p&255;bmp[k+1]=(p>>8)&255;bmp[k+2]=(p>>16)&255;
  }
  web.sendHeader("X-Frame-Seq",String(sequence));web.sendHeader("Cache-Control","no-store");
  web.setContentLength(54+96*96*3);web.send(200,"image/bmp","");web.client().write(bmp,54+96*96*3);free(bmp);
}
bool cameraInit(){
  camera_config_t c={};c.ledc_channel=LEDC_CHANNEL_0;c.ledc_timer=LEDC_TIMER_0;
  c.pin_d0=Y2_GPIO_NUM;c.pin_d1=Y3_GPIO_NUM;c.pin_d2=Y4_GPIO_NUM;c.pin_d3=Y5_GPIO_NUM;
  c.pin_d4=Y6_GPIO_NUM;c.pin_d5=Y7_GPIO_NUM;c.pin_d6=Y8_GPIO_NUM;c.pin_d7=Y9_GPIO_NUM;
  c.pin_xclk=XCLK_GPIO_NUM;c.pin_pclk=PCLK_GPIO_NUM;c.pin_vsync=VSYNC_GPIO_NUM;c.pin_href=HREF_GPIO_NUM;
  c.pin_sscb_sda=SIOD_GPIO_NUM;c.pin_sscb_scl=SIOC_GPIO_NUM;c.pin_pwdn=PWDN_GPIO_NUM;c.pin_reset=RESET_GPIO_NUM;
  c.xclk_freq_hz=10000000;c.pixel_format=PIXFORMAT_JPEG;c.frame_size=FRAMESIZE_UXGA;c.jpeg_quality=12;c.fb_count=1;
  if(esp_camera_init(&c)!=ESP_OK)return false;
  sensor_t*s=esp_camera_sensor_get();s->set_framesize(s,FRAMESIZE_QVGA);s->set_vflip(s,0);s->set_hmirror(s,0);
  s->set_special_effect(s,0);s->set_whitebal(s,1);s->set_awb_gain(s,1);s->set_wb_mode(s,0);
  s->set_exposure_ctrl(s,1);s->set_gain_ctrl(s,1);s->set_saturation(s,0);
  return true;
}
void loadCalibration(int resolution){
 calibrationResolution=resolution;for(int i=0;i<5;i++)distanceK[i]=0;
 if(calibrationReady && calibration.getInt("res",-1)==resolution)for(int i=0;i<5;i++){float k=calibration.getFloat(calibrationKeys[i],0);if(std::isfinite(k)&&k>=0&&k<=1000)distanceK[i]=k;}
}
String makeCameraStatus(){
 sensor_t *s=esp_camera_sensor_get();String j="{";
 j+="\"framesize\":"+String((int)s->status.framesize);
 j+=",\"quality\":"+String((int)s->status.quality);
 j+=",\"brightness\":"+String((int)s->status.brightness);
 j+=",\"contrast\":"+String((int)s->status.contrast);
 j+=",\"saturation\":"+String((int)s->status.saturation);
 j+=",\"special_effect\":"+String((int)s->status.special_effect);
 j+=",\"wb_mode\":"+String((int)s->status.wb_mode);
 j+=",\"awb\":"+String((int)s->status.awb);
 j+=",\"awb_gain\":"+String((int)s->status.awb_gain);
 j+=",\"aec\":"+String((int)s->status.aec);
 j+=",\"aec2\":"+String((int)s->status.aec2);
 j+=",\"ae_level\":"+String((int)s->status.ae_level);
 j+=",\"aec_value\":"+String((int)s->status.aec_value);
 j+=",\"agc\":"+String((int)s->status.agc);
 j+=",\"agc_gain\":"+String((int)s->status.agc_gain);
 j+=",\"gainceiling\":"+String((int)s->status.gainceiling);
 j+=",\"bpc\":"+String((int)s->status.bpc);
 j+=",\"wpc\":"+String((int)s->status.wpc);
 j+=",\"raw_gma\":"+String((int)s->status.raw_gma);
 j+=",\"lenc\":"+String((int)s->status.lenc);
 j+=",\"vflip\":"+String((int)s->status.vflip);
 j+=",\"hmirror\":"+String((int)s->status.hmirror);
 j+=",\"dcw\":"+String((int)s->status.dcw);
 j+=",\"colorbar\":"+String((int)s->status.colorbar);
 j+=",\"face_detect\":"+String(faceEnabled)+",\"face_recognize\":"+String(faceRecognize)+",\"face_enroll\":"+String(faceEnroll)+",\"color_detect\":"+String((bool)colorEnabled);
 j+=",\"detect\":"+String((bool)detectEnabled)+",\"input_bgr\":"+String(inputBGR)+",\"view_mode\":"+String(viewMode)+",\"view_cx\":"+String(viewCx)+",\"view_cy\":"+String(viewCy)+",\"threshold\":"+String((int)(aiThreshold*100+0.5f));
 j+=",\"sensor_pid\":"+String(s->id.PID)+",\"firmware\":\"A2 Camera FullUI 3\"}";return j;
}
bool cameraRange(const String &key,int val){
 if(key=="framesize")return val==0||(val>=3&&val<=10);
 if(key=="quality")return val>=10&&val<=63;
 if(key=="brightness"||key=="contrast"||key=="saturation"||key=="ae_level")return val>=-2&&val<=2;
 if(key=="agc_gain")return val>=0&&val<=30;
 if(key=="aec_value")return val>=0&&val<=1200;
 if(key=="special_effect"||key=="gainceiling")return val>=0&&val<=6;
 if(key=="wb_mode")return val>=0&&val<=4;
 if(key=="view_mode")return val>=0&&val<=3;
 if(key=="view_cx"||key=="view_cy")return val>=0&&val<=1000;
 if(key=="threshold")return val>=50&&val<=99;
 return val==0||val==1;
}
void cameraControl(){
 String key=web.arg("var"),value=web.arg("val");char *end=nullptr;long n=strtol(value.c_str(),&end,10);
 if(!value.length()||value.length()>6||*end||!cameraRange(key,n)){web.send(400,"text/plain","Invalid setting/value");return;}
 sendJSON("{\"N\":100}");owner=0;
 if(xSemaphoreTake(frameMutex,pdMS_TO_TICKS(20))!=pdTRUE){web.send(503,"text/plain","Vision busy; retry");return;}
 sensor_t *s=esp_camera_sensor_get();int val=n,res=0;
 if(key=="framesize"){
  if(frame){esp_camera_fb_return(frame);frame=nullptr;}
  res=s->set_framesize(s,(framesize_t)val);
  if(!res){loadCalibration(val);inputReady=false;geometryEpoch++;xSemaphoreTake(stateMutex,portMAX_DELAY);free(completedJpeg);completedJpeg=nullptr;completedLen=0;cachedResults="{\"seq\":0,\"valid\":0,\"objects\":[]}";xSemaphoreGive(stateMutex);}
 }
 else if(key=="quality")res=s->set_quality(s,val);
 else if(key=="contrast")res=s->set_contrast(s,val);
 else if(key=="brightness")res=s->set_brightness(s,val);
 else if(key=="saturation")res=s->set_saturation(s,val);
 else if(key=="gainceiling")res=s->set_gainceiling(s,(gainceiling_t)val);
 else if(key=="colorbar")res=s->set_colorbar(s,val);
 else if(key=="awb")res=s->set_whitebal(s,val);
 else if(key=="agc")res=s->set_gain_ctrl(s,val);
 else if(key=="aec")res=s->set_exposure_ctrl(s,val);
 else if(key=="hmirror")res=s->set_hmirror(s,val);
 else if(key=="vflip")res=s->set_vflip(s,val);
 else if(key=="awb_gain")res=s->set_awb_gain(s,val);
 else if(key=="agc_gain")res=s->set_agc_gain(s,val);
 else if(key=="aec_value")res=s->set_aec_value(s,val);
 else if(key=="aec2")res=s->set_aec2(s,val);
 else if(key=="dcw")res=s->set_dcw(s,val);
 else if(key=="bpc")res=s->set_bpc(s,val);
 else if(key=="wpc")res=s->set_wpc(s,val);
 else if(key=="raw_gma")res=s->set_raw_gma(s,val);
 else if(key=="lenc")res=s->set_lenc(s,val);
 else if(key=="special_effect")res=s->set_special_effect(s,val);
 else if(key=="wb_mode")res=s->set_wb_mode(s,val);
 else if(key=="ae_level")res=s->set_ae_level(s,val);
 else if(key=="color_detect")colorEnabled=val;
 else if(key=="detect")detectEnabled=val;
 else if(key=="face_detect"){faceEnabled=val;if(!val){faceRecognize=false;faceEnroll=false;}}
 else if(key=="face_recognize"){faceRecognize=val;if(val)faceEnabled=true;}
 else if(key=="face_enroll"){faceEnroll=val;if(val)faceEnabled=true;}
 else if(key=="input_bgr")inputBGR=val;
 else if(key=="view_mode")viewMode=val;
 else if(key=="view_cx")viewCx=val;
 else if(key=="view_cy")viewCy=val;
 else if(key=="threshold")aiThreshold=val/100.0f;
 else res=-1;
 if(!res)geometryEpoch++;
 xSemaphoreTake(stateMutex,portMAX_DELAY);cameraStatus=makeCameraStatus();xSemaphoreGive(stateMutex);
 xSemaphoreGive(frameMutex);
 web.send(res?400:200,"text/plain",res?"Unsupported setting / sensor rejected value":"Applied; vehicle stopped");
}

void visionTask(void*){
 for(;;){
  if(xSemaphoreTake(frameMutex,portMAX_DELAY)==pdTRUE){
   acquire();String result=results();
   result.remove(result.length()-1);result+=",\"detect_enabled\":"+String((bool)detectEnabled)+",\"color_enabled\":"+String((bool)colorEnabled)+"}";
   uint8_t *next=nullptr;size_t n=0;if(cameraOK){n=frame->len;next=(uint8_t*)ps_malloc(n);if(next)memcpy(next,frame->buf,n);}
   xSemaphoreTake(stateMutex,portMAX_DELAY);cachedResults=result;free(completedJpeg);completedJpeg=next;completedLen=next?n:0;cameraStatus=makeCameraStatus();xSemaphoreGive(stateMutex);
   xSemaphoreGive(frameMutex);Serial.println(result);
  }
  vTaskDelay(pdMS_TO_TICKS(FRAME_INTERVAL_MS));
 }
}
bool strictInt(const String &s,int lo,int hi,int &v){
 if(!s.length()||s.length()>5)return false;
 for(size_t i=0;i<s.length();i++)if(s[i]<'0'||s[i]>'9')return false;
 v=s.toInt();return v>=lo&&v<=hi;
}
void webCommand(){
 String action=web.arg("action");String j;
 int value;
 if(action=="stop"){j="{\"N\":100}";owner=0;}
 else if(action=="ping"){if(owner==1)sendJSON("{\"N\":103}");web.send(200,"text/plain","ok");return;}
 else if(action=="move"){
  int speed;if(!strictInt(web.arg("dir"),1,9,value)||!strictInt(web.arg("speed"),0,180,speed)){web.send(400,"text/plain","Bad direction/speed");return;}
  j="{\"N\":102,\"D1\":"+String(value)+",\"D2\":"+String(speed)+"}";owner=value==9?0:1;
 }else if(action=="mode"){
  if(!strictInt(web.arg("value"),0,4,value)){web.send(400,"text/plain","Bad mode");return;}
  if(value==4&&(!A2_ENABLE_AI||!detectEnabled)){web.send(409,"text/plain","Import model and enable AI first");return;}
  j="{\"N\":101,\"D1\":"+String(value)+"}";owner=value==0?0:1;
 }else if(action=="speed"){
  if(!strictInt(web.arg("value"),0,180,value)){web.send(400,"text/plain","Bad speed");return;}
  j="{\"N\":104,\"D1\":"+String(value)+"}";
 }else if(action=="servo"){
  if(!strictInt(web.arg("value"),0,180,value)){web.send(400,"text/plain","Bad servo angle");return;}
  owner=0;j="{\"N\":5,\"D1\":1,\"D2\":"+String(value)+"}";
 }else if(action=="led"){
  int r,g,b;if(!strictInt(web.arg("r"),0,255,r)||!strictInt(web.arg("g"),0,255,g)||!strictInt(web.arg("b"),0,255,b)){web.send(400,"text/plain","Bad RGB");return;}
  j="{\"N\":8,\"D1\":0,\"D2\":"+String(r)+",\"D3\":"+String(g)+",\"D4\":"+String(b)+"}";
 }else if(action=="guard"){
  if(!strictInt(web.arg("value"),0,1,value)){web.send(400,"text/plain","Bad guard flag");return;}
  if(value&&(!A2_ENABLE_AI||!detectEnabled)){web.send(409,"text/plain","No AI model");return;}
  j="{\"N\":201,\"D1\":"+String(value)+"}";owner=0;
 }else if(action=="reset_stop"){j="{\"N\":202}";owner=0;}
 else if(action=="detect"||action=="color"){
  if(!strictInt(web.arg("value"),0,1,value)){web.send(400,"text/plain","Bad flag");return;}
  if(action=="detect"&&value&&!A2_ENABLE_AI){web.send(409,"text/plain","Firmware has no model");return;}
  // Changing either analysis setting stops the car first.
  sendJSON("{\"N\":100}");owner=0;
  if(action=="detect")detectEnabled=value;else colorEnabled=value;
  web.send(200,"text/plain","Changed; vehicle stopped");return;
 }else{web.send(400,"text/plain","Unknown action");return;}
 sendJSON(j);web.send(200,"application/json",j);
}
void calibrateDistance(){
 int id,width,cm,referenceW,resolution,epoch;int slot;
 if(!strictInt(web.arg("id"),1,9,id)||(slot=calibrationSlot(id))<0){web.send(400,"text/plain","Invalid object type");return;}
 bool clear=web.arg("clear")=="1";
 if(!clear && (!strictInt(web.arg("width"),6,ANALYSIS_MAX_W-2,width)||!strictInt(web.arg("cm"),5,1000,cm)||!strictInt(web.arg("image_w"),100,400,referenceW)||!strictInt(web.arg("framesize"),0,10,resolution)||!strictInt(web.arg("epoch"),0,65535,epoch))){web.send(400,"text/plain","Need sampled width and known distance 5-1000 cm");return;}
 sendJSON("{\"N\":100}");owner=0;
 if(xSemaphoreTake(frameMutex,pdMS_TO_TICKS(20))!=pdTRUE){web.send(503,"text/plain","Vision busy; press Save again");return;}
 if(!clear && (resolution!=(int)esp_camera_sensor_get()->status.framesize || epoch!=geometryEpoch || referenceW!=analysisW)){xSemaphoreGive(frameMutex);web.send(409,"text/plain","Camera settings changed. Sample again.");return;}
 float k=clear?0.0f:(float)width*cm/referenceW;
 bool saved=calibrationReady;
 if(saved){if(calibration.getInt("res",-1)!=calibrationResolution){for(int i=0;i<5;i++)saved=(calibration.putFloat(calibrationKeys[i],0)==sizeof(float))&&saved;}saved=(calibration.putInt("res",calibrationResolution)==sizeof(int))&&saved;saved=(calibration.putFloat(calibrationKeys[slot],k)==sizeof(float))&&saved;}
 if(saved)distanceK[slot]=k;
 xSemaphoreGive(frameMutex);
 if(!saved){web.send(500,"text/plain","Calibration could not be stored");return;}
 web.send(200,"text/plain",String(clear?"Calibration cleared. ":"Calibration saved. ")+"K="+String(k,2)+" cm (normalized width); vehicle stopped");
}
void bridge(){
 if(appServer.hasClient()){
  WiFiClient c=appServer.available();if(appClient&&appClient.connected())c.stop();else{appClient=c;appRx="";appLastSeen=millis();}
 }
 int budget=256;
 while(appClient&&appClient.available()&&budget--){
  char c=appClient.read();if(c=='{')appRx="";
  if(appRx.length()<180)appRx+=c;else appRx="";
  if(c=='}'){
   appLastSeen=millis();
   if(appRx=="{Heartbeat}"){if(owner==2)sendJSON("{\"N\":103}");}
   else if(appRx.startsWith("{")&&appRx.indexOf("\"N\"")>=0){owner=2;sendJSON(appRx);}
   appRx="";
  }
 }
 if(appClient&&appClient.connected()){
  if(millis()-appHeartbeat>1000){appHeartbeat=millis();appClient.print("{Heartbeat}");}
  if(millis()-appLastSeen>3000){if(owner==2){sendJSON("{\"N\":100}");owner=0;}appClient.stop();}
 }else if(owner==2){sendJSON("{\"N\":100}");owner=0;}
 while(Serial2.available()){
  char c=Serial2.read();if(c=='{')unoRx="";
  if(unoRx.length()<250)unoRx+=c;else unoRx="";
  if(c=='}'){
   if(unoRx.startsWith("{\"N\":210,")){unoStatus=unoRx;lastUno=millis();}
   else lastReply=unoRx;
   if(appClient&&appClient.connected())appClient.print(unoRx);
   unoRx="";
  }
 }
}
void setup(){
 Serial.begin(115200);Serial2.begin(9600,SERIAL_8N1,UART_RX,UART_TX);sendJSON("{\"N\":100}");
 frameMutex=xSemaphoreCreateMutex();stateMutex=xSemaphoreCreateMutex();visionQueue=xQueueCreate(1,sizeof(VisionPayload));
 if(!frameMutex||!stateMutex||!visionQueue||!previewInit()){Serial.println("ERROR synchronization allocation");while(true)delay(1000);}
#if A2_ENABLE_AI
 if(strcmp(ei_classifier_inferencing_categories[0],"3")){
  Serial.println("ERROR: expected model target class 3");while(true)delay(1000);
 }
#endif
 if(!psramFound()){Serial.println("ERROR: enable PSRAM");while(true)delay(1000);}
 rgb=(uint8_t*)ps_malloc(ANALYSIS_MAX_W*ANALYSIS_MAX_H*3);mask=(uint8_t*)ps_malloc(ANALYSIS_MAX_W*ANALYSIS_MAX_H);
 queueBuffer=(uint32_t*)ps_malloc(ANALYSIS_MAX_W*ANALYSIS_MAX_H*sizeof(uint32_t));
 if(!rgb||!mask||!queueBuffer||!cameraInit()){Serial.println("ERROR: camera/memory");while(true)delay(1000);}
 calibrationReady=calibration.begin("a2-cam-v2",false);
 loadCalibration(4);initFaces();
 detectEnabled=A2_ENABLE_AI;
 WiFi.mode(WIFI_AP);WiFi.softAP(AP_SSID,AP_PASSWORD,AP_CHANNEL);
 web.on("/",HTTP_GET,[]{web.send_P(200,"text/html",PAGE);});
 web.on("/results",HTTP_GET,[]{
  xSemaphoreTake(stateMutex,portMAX_DELAY);String s=cachedResults;xSemaphoreGive(stateMutex);
  if(s.endsWith("}"))s.remove(s.length()-1);
  s+=",\"uno\":"+unoStatus+",\"uno_age_ms\":"+String(millis()-lastUno)+",\"detect_enabled\":"+String((bool)detectEnabled)+",\"color_enabled\":"+String((bool)colorEnabled)+"}";
  web.sendHeader("Cache-Control","no-store");web.send(200,"application/json",s);
 });
 web.on("/capture",HTTP_GET,[]{
  uint8_t *jpeg=nullptr;size_t length=0;uint16_t seq=0;
  if(!previewCopy(jpeg,length,seq)){web.send(503,"text/plain","No camera JPEG yet");return;}
  web.sendHeader("Cache-Control","no-store");web.sendHeader("X-Frame-Seq",String(seq));
  web.setContentLength(length);web.send(200,"image/jpeg","");web.client().write(jpeg,length);free(jpeg);
 });
 web.on("/sample",HTTP_GET,[]{
  if(xSemaphoreTake(frameMutex,pdMS_TO_TICKS(10))!=pdTRUE){web.send(503,"text/plain","Processing; retry");return;}
  sample();xSemaphoreGive(frameMutex);
 });
 web.on("/frame",HTTP_GET,[]{
  uint8_t *copy=nullptr;size_t n=0;String meta;
  xSemaphoreTake(stateMutex,portMAX_DELAY);if(completedJpeg){n=completedLen;copy=(uint8_t*)ps_malloc(n);if(copy){memcpy(copy,completedJpeg,n);meta=cachedResults;}}xSemaphoreGive(stateMutex);
  if(!copy){web.send(503,"text/plain","Waiting for a completed analysis frame");return;}
  web.sendHeader("Cache-Control","no-store");web.sendHeader("X-Analysis",meta);web.setContentLength(n);web.send(200,"image/jpeg","");web.client().write(copy,n);free(copy);
 });
 web.on("/model-input",HTTP_GET,[]{
  if(xSemaphoreTake(frameMutex,pdMS_TO_TICKS(20))!=pdTRUE){web.send(503,"text/plain","Vision busy; retry");return;}
  uint8_t *copy=inputReady?(uint8_t*)malloc(sizeof(inputBMP)):nullptr;if(copy)memcpy(copy,inputBMP,sizeof(inputBMP));uint16_t seq=sequence;xSemaphoreGive(frameMutex);
  if(!copy){web.send(503,"text/plain","No model input");return;}web.sendHeader("X-Frame-Seq",String(seq));web.sendHeader("Cache-Control","no-store");web.setContentLength(sizeof(inputBMP));web.send(200,"image/bmp","");web.client().write(copy,sizeof(inputBMP));free(copy);
 });
 web.on("/status",HTTP_GET,[]{xSemaphoreTake(stateMutex,portMAX_DELAY);String s=cameraStatus;xSemaphoreGive(stateMutex);web.sendHeader("Cache-Control","no-store");web.send(200,"application/json",s);});
 web.on("/control",HTTP_GET,cameraControl);web.on("/control",HTTP_POST,cameraControl);
 web.on("/cmd",HTTP_POST,webCommand);
 web.on("/calibrate",HTTP_POST,calibrateDistance);
 web.begin();appServer.begin();startStream();
 if(xTaskCreatePinnedToCore(visionTask,"vision",16384,nullptr,1,nullptr,0)!=pdPASS){Serial.println("ERROR vision task");while(true)delay(1000);}
 Serial.println("Dashboard http://192.168.4.1 ; MJPEG http://192.168.4.1:81/stream ; APP TCP100");
}
void loop(){
 web.handleClient();bridge();
 VisionPayload v;
 if(xQueueReceive(visionQueue,&v,0)==pdTRUE){
  char s[160];snprintf(s,sizeof(s),"{\"N\":200,\"S\":%u,\"A\":%d,\"V\":%d,\"P\":%d,\"R\":%d,\"X\":%d,\"Y\":%d,\"W\":%d,\"H\":%d}",
   v.seq,v.ai,(v.valid&&millis()-v.captured<=2000),v.confidence,v.distance,v.cx,v.cy,v.w,v.h);
  sendJSON(s);
 }
 delay(1);
}
