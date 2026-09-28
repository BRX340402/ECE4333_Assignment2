#include <cassert>
#include "../ESP32_Assignment2/VisionGeometry.h"
#include "../ESP32_Assignment2/color_blob.h"
#include <vector>
bool inputBGR=true;
int main(){
 for(auto wh: {std::pair<int,int>{160,120},{240,176},{320,240},{400,296},{400,300},{320,256},{256,192},{200,150}}){
  for(int m=0;m<4;m++)for(int cx: {0,500,1000})for(int cy: {0,500,1000}){auto v=makeView(wh.first,wh.second,m,cx,cy);assert(v.x>=0&&v.y>=0&&v.x+v.w<=wh.first&&v.y+v.h<=wh.second);for(int y=0;y<96;y++)for(int x=0;x<96;x++){int sx,sy;if(viewPixel(v,x,y,sx,sy))assert(sx>=0&&sy>=0&&sx<wh.first&&sy<wh.second);}}
 }
 auto v=makeView(320,240,0,500,500);int x,y;assert(v.outH==72&&v.padY==12);assert(!viewPixel(v,1,0,x,y));assert(viewPixel(v,0,12,x,y)&&x<4);assert(viewPixel(v,95,83,x,y)&&x>315);
 v=makeView(320,240,1,500,500);assert(v.x==40&&v.w==240);v=makeView(320,240,2,1000,1000);assert(v.x==200&&v.y==120&&v.w==120);
 for(int w: {160,320,400}){int h=w*3/4;std::vector<uint8_t> rgb(w*h*3),mask(w*h);std::vector<uint32_t> q(w*h);int left=w/2-10,top=h/2-10;
  for(int y=top;y<top+20;y++)for(int x=left;x<left+20;x++)rgb[3*(y*w+x)+2]=255;
  for(int y=2;y<32;y++)for(int x=2;x<32;x++)rgb[3*(y*w+x)+2]=255;
  auto b=find_color_blob(rgb.data(),w,h,mask.data(),q.data(),TARGET_RED,w/2,h/2);assert(b.x0==left&&b.y0==top&&b.x1-left+1==20);
 }
}
