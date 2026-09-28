#pragma once
#include "Config.h"
#include "color_blob.h"
#include "VisionGeometry.h"
#include <string.h>
#if A2_ENABLE_AI
#define EIDSP_USE_ESP_DSP 0
#include A2_MODEL_HEADER
static_assert(EI_CLASSIFIER_OBJECT_DETECTION==1 && EI_HAS_FOMO==1,"FOMO required");
static_assert(EI_CLASSIFIER_INPUT_WIDTH==96 && EI_CLASSIFIER_INPUT_HEIGHT==96,"96x96 model required");
#endif
extern int analysisW,analysisH;
extern bool inputBGR;
extern ViewMap aiView;
extern float aiThreshold;
static const char *className(int id){switch(id){case 1:return "stop";case 6:return "red";case 7:return "blue";case 8:return "green";case 9:return "orange";default:return "other";}}
struct Crop{int x,y,side;};
static Crop cropFor(const ColorBlob &b){int w=b.x1-b.x0+1,h=b.y1-b.y0+1;int side=(w>h?w:h)*13/10+2;return {(b.x0+b.x1+1-side)/2,(b.y0+b.y1+1-side)/2,side};}
static uint32_t packedPixel(const uint8_t *p){return ((uint32_t)p[inputBGR?2:0]<<16)|((uint32_t)p[1]<<8)|p[inputBGR?0:2];}
static uint32_t cropPixel(const uint8_t *pixels,const Crop &c,int x,int y){int sx=c.x+(2*x+1)*c.side/192,sy=c.y+(2*y+1)*c.side/192;if(sx<0||sy<0||sx>=analysisW||sy>=analysisH)return 0;return packedPixel(pixels+3*(sy*analysisW+sx));}
static const int MAX_STOPS=4;
struct StopPrediction{ColorBlob box[MAX_STOPS];float confidence[MAX_STOPS];int count;float best;uint32_t duration;int error;};
static const uint8_t *aiPixels;
static int signalData(size_t offset,size_t length,float *out){if(offset>9216||length>9216-offset)return -1;for(size_t i=0;i<length;i++){int x,y;size_t n=offset+i;out[i]=viewPixel(aiView,n%96,n/96,x,y)?(float)packedPixel(aiPixels+3*(y*analysisW+x)):0;}return 0;}
static bool inferFrame(const uint8_t *pixels,StopPrediction &out){
 out={};aiPixels=pixels;
#if A2_ENABLE_AI
 ei::signal_t sig;sig.total_length=9216;sig.get_data=signalData;ei_impulse_result_t result={};uint32_t start=millis();
 auto err=run_classifier(&sig,&result,false);out.duration=millis()-start;out.error=(int)err;if(err!=EI_IMPULSE_OK)return false;
 for(uint32_t i=0;i<result.bounding_boxes_count;i++){
  const auto &b=result.bounding_boxes[i];if(!b.label||strcmp(b.label,"3")||b.value<=0)continue;
  if(b.value>out.best)out.best=b.value;
  if(b.value<aiThreshold)continue;
  int mx=b.x+b.width/2,my=b.y+b.height/2,x,y;if(!viewPixel(aiView,mx,my,x,y))continue;
  int slot=out.count;if(slot>=MAX_STOPS){slot=0;for(int j=1;j<MAX_STOPS;j++)if(out.confidence[j]<out.confidence[slot])slot=j;if(b.value<=out.confidence[slot])continue;}else out.count++;
  int w=(b.width*aiView.w+aiView.outW-1)/aiView.outW,h=(b.height*aiView.h+aiView.outH-1)/aiView.outH;
  int x0=boundi(x-w/2,0,analysisW-1),y0=boundi(y-h/2,0,analysisH-1);
  out.box[slot]={x,y,x0,y0,boundi(x0+w-1,0,analysisW-1),boundi(y0+h-1,0,analysisH-1),0};out.confidence[slot]=b.value;
 }
#endif
 return true;
}
