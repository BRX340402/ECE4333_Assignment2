#pragma once
#include "Config.h"
#include "color_blob.h"
#include <string.h>
#if A2_ENABLE_AI
#define EIDSP_USE_ESP_DSP 0
#include A2_MODEL_HEADER
#if EI_CLASSIFIER_OBJECT_DETECTION != 1 || EI_HAS_FOMO != 1
#error "This firmware requires the supplied FOMO object detection model."
#endif
static_assert(EI_CLASSIFIER_INPUT_WIDTH == 96 && EI_CLASSIFIER_INPUT_HEIGHT == 96,
              "Expected the supplied 96x96 model");
static_assert(EI_CLASSIFIER_LABEL_COUNT == 1, "Expected one target class");
#endif
// Internal robot protocol: 1 = STOP sign; 6..9 = colour only.
static const char *className(int id) {
  switch (id) {
    case 1: return "stop";
    case 6: return "red";
    case 7: return "blue";
    case 8: return "green";
    case 9: return "orange";
    default: return "other";
  }
}
struct Crop { int x, y, side; };
static Crop cropFor(const ColorBlob &b) {
  int w=b.x1-b.x0+1,h=b.y1-b.y0+1;
  int side=(w>h?w:h)*13/10+2;
  return {(b.x0+b.x1+1-side)/2,(b.y0+b.y1+1-side)/2,side};
}
static uint32_t cropPixel(const uint8_t *pixels,const Crop &c,int x,int y) {
  int sx=c.x+(2*x+1)*c.side/192,sy=c.y+(2*y+1)*c.side/192;
  if(sx<0||sy<0||sx>=IMAGE_W||sy>=IMAGE_H)return 0;
  const uint8_t *p=pixels+3*(sy*IMAGE_W+sx);
  return ((uint32_t)p[COLOR_INPUT_BGR?2:0]<<16)|((uint32_t)p[1]<<8)|p[COLOR_INPUT_BGR?0:2];
}
struct StopPrediction { ColorBlob box; float confidence; uint32_t duration; int error; bool found; };
#if A2_ENABLE_AI
static const uint8_t *aiPixels;
// "Fit shortest axis": centre crop QVGA to 240x240 and resize to 96x96.
static int signalData(size_t offset,size_t length,float *out) {
  if(offset>96u*96u || length>96u*96u-offset)return -1;
  const int side=(IMAGE_W<IMAGE_H?IMAGE_W:IMAGE_H);
  const int x0=(IMAGE_W-side)/2,y0=(IMAGE_H-side)/2;
  for(size_t i=0;i<length;i++) {
    size_t n=offset+i;
    int sx=x0+((2*(int)(n%96)+1)*side)/(2*96);
    int sy=y0+((2*(int)(n/96)+1)*side)/(2*96);
    const uint8_t *p=aiPixels+3*(sy*IMAGE_W+sx);
    out[i]=(float)(((uint32_t)p[COLOR_INPUT_BGR?2:0]<<16)|
                   ((uint32_t)p[1]<<8)|p[COLOR_INPUT_BGR?0:2]);
  }
  return 0;
}
#endif
static bool inferFrame(const uint8_t *pixels,StopPrediction &stop) {
  stop.found=false;stop.confidence=0;stop.duration=0;stop.error=0;
#if A2_ENABLE_AI
  aiPixels=pixels;
  ei::signal_t signal;signal.total_length=96*96;signal.get_data=signalData;
  ei_impulse_result_t result={};uint32_t start=millis();
  EI_IMPULSE_ERROR err=run_classifier(&signal,&result,false);
  stop.duration=millis()-start;
  stop.error=(int)err;
  if(err!=EI_IMPULSE_OK){Serial.printf("AI error: %d\n",(int)err);return false;}
  const int side=(IMAGE_W<IMAGE_H?IMAGE_W:IMAGE_H);
  const int xoff=(IMAGE_W-side)/2,yoff=(IMAGE_H-side)/2;
  for(uint32_t i=0;i<result.bounding_boxes_count;i++) {
    const ei_impulse_result_bounding_box_t &b=result.bounding_boxes[i];
    if(!b.label || strcmp(b.label,"3") || b.value<AI_THRESHOLD || b.value<=stop.confidence)continue;
    int x=xoff+(int)b.x*side/96,y=yoff+(int)b.y*side/96;
    int w=((int)b.width*side+95)/96,h=((int)b.height*side+95)/96;
    if(w<1)w=1;if(h<1)h=1;
    int x1=x+w-1,y1=y+h-1;
    if(x<0)x=0;if(y<0)y=0;
    if(x1>=IMAGE_W)x1=IMAGE_W-1;if(y1>=IMAGE_H)y1=IMAGE_H-1;
    if(x>x1||y>y1)continue;
    stop.box={(x+x1)/2,(y+y1)/2,x,y,x1,y1,0};
    stop.confidence=b.value;stop.found=true;
  }
#endif
  return true;
}
