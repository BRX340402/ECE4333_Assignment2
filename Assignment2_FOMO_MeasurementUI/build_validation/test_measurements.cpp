#include <cassert>
#include <cmath>
#include <vector>
#include "../ESP32_Assignment2/color_blob.h"
using std::isfinite;
const int IMAGE_W=320,IMAGE_H=240;
float distanceK[5]={0,0,0,0,0};
int calibrationSlot(int id){return id==1?0:(id>=6&&id<=9?id-5:-1);}
bool usableSize(const ColorBlob &b){return b.pixels && b.x1-b.x0+1>=6 && b.x0>0 && b.y0>0 && b.x1<IMAGE_W-1 && b.y1<IMAGE_H-1;}
int distanceFor(int id,const ColorBlob &b){
  int slot=calibrationSlot(id);
  float k=slot>=0?distanceK[slot]:0;
  int w=b.x1-b.x0+1;
  if(k<=0 || !usableSize(b))return -1;
  float d=k/w;if(!isfinite(d)||d>10000)return -1;return (int)(d+0.5f);
}
int main(){
 const int w=320,h=240;std::vector<uint8_t> rgb(w*h*3),mask(w*h);std::vector<uint32_t> q(w*h);
 auto red=[&](int x,int y){rgb[3*(y*w+x)+2]=255;};
 // Smaller red ring around a white sign centre; larger unrelated red distractor.
 for(int y=80;y<120;y++)for(int x=120;x<160;x++)if(x<124||x>=156||y<84||y>=116)red(x,y);
 for(int y=10;y<60;y++)for(int x=10;x<80;x++)red(x,y);
 ColorBlob largest=find_color_blob(rgb.data(),w,h,mask.data(),q.data(),TARGET_RED);
 assert(largest.x0==10);
 ColorBlob sign=find_color_blob(rgb.data(),w,h,mask.data(),q.data(),TARGET_RED,140,100);
 assert(sign.x0==120 && sign.x1==159 && sign.y0==80 && sign.y1==119);
 assert(distanceFor(1,sign)==-1);
 distanceK[0]=50*40;assert(distanceFor(1,sign)==50);
 sign.x1=139;assert(distanceFor(1,sign)==100);
 sign.x0=0;assert(distanceFor(1,sign)==-1);
 sign={140,100,120,80,159,119,0};assert(distanceFor(1,sign)==-1); // FOMO location cannot give size
 assert(!find_color_blob(rgb.data(),w,h,mask.data(),q.data(),TARGET_RED,290,210).pixels);
 distanceK[1]=3500;assert(distanceFor(6,largest)==50);assert(distanceFor(7,largest)==-1);
}
