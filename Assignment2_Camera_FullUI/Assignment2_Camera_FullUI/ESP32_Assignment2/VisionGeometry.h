#pragma once
#include <stdint.h>
struct ViewMap { int x,y,w,h; int padX,padY,outW,outH; };
inline int boundi(int v,int lo,int hi){return v<lo?lo:v>hi?hi:v;}
// mode 0: full frame letterbox; 1: original centre square; 2/3: movable 2x/4x square.
inline ViewMap makeView(int w,int h,int mode,int cxPermille,int cyPermille){
 ViewMap v={0,0,w,h,0,0,96,96};
 if(mode){int side=w<h?w:h;if(mode==2)side/=2;if(mode==3)side/=4;
  v.w=v.h=side;v.x=boundi(w*cxPermille/1000-side/2,0,w-side);v.y=boundi(h*cyPermille/1000-side/2,0,h-side);
 }else if(w>=h){v.outH=(96*h+w/2)/w;v.padY=(96-v.outH)/2;}
 else {v.outW=(96*w+h/2)/h;v.padX=(96-v.outW)/2;}
 return v;
}
inline bool viewPixel(const ViewMap &v,int mx,int my,int &x,int &y){
 if(mx<v.padX||my<v.padY||mx>=v.padX+v.outW||my>=v.padY+v.outH)return false;
 x=v.x+((2*(mx-v.padX)+1)*v.w)/(2*v.outW);y=v.y+((2*(my-v.padY)+1)*v.h)/(2*v.outH);return true;
}
