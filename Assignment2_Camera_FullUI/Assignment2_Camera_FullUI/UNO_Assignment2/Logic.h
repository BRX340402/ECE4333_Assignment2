#pragma once
#include <stdint.h>
#include "Hardware.h"
struct VisionState {
  bool ai=false,valid=false,seen=false,latched=false;
  uint16_t seq=0;uint32_t time=0;
  int confidence=0,distance=-1,cx=-1,cy=-1,w=0,h=0;
  bool fresh(uint32_t now)const{return seen&&ai&&valid&&(uint32_t)(now-time)<AI_TIMEOUT_MS;}
  bool update(uint16_t s,bool a,bool v,int p,int d,int x,int y,int width,int height,uint32_t now){
    if(seen&&(uint32_t)(now-time)<AI_TIMEOUT_MS){uint16_t delta=s-seq;if(!delta||delta>=32768)return false;}
    seq=s;ai=a;valid=v;confidence=p;distance=d;cx=x;cy=y;w=width;h=height;time=now;seen=true;return true;
  }
  void evaluate(bool guard,uint32_t now){
    if(guard&&fresh(now)&&confidence>=800&&(distance<0||distance<=STOP_DISTANCE_CM))latched=true;
  }
  bool reset(uint32_t now){
    if(!fresh(now)||(confidence>=800&&(distance<0||distance<=STOP_DISTANCE_CM)))return false;
    latched=false;return true;
  }
};
static void directionPWM(int dir,int speed,int &left,int &right){
 left=right=0;
 switch(dir){case 1:left=right=speed;break;case 2:left=right=-speed;break;
 case 3:left=-speed;right=speed;break;case 4:left=speed;right=-speed;break;
 case 5:left=speed/2;right=speed;break;case 6:left=-speed/2;right=-speed;break;
 case 7:left=speed;right=speed/2;break;case 8:left=-speed;right=-speed/2;break;}
}
