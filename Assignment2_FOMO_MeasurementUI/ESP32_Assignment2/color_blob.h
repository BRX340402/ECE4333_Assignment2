#ifndef COLOR_BLOB_H
#define COLOR_BLOB_H
#include <stdint.h>
#include <stdlib.h>
// Decoder byte order for this legacy camera library. Set to 0 if red/blue are swapped.
#ifndef COLOR_INPUT_BGR
#define COLOR_INPUT_BGR 1
#endif
#define COLOR_MIN_PIXELS 100
struct ColorBlob { int x, y, x0, y0, x1, y1; uint32_t pixels; };
enum TargetColor { TARGET_RED, TARGET_BLUE, TARGET_GREEN, TARGET_ORANGE };
// HSV: red 0 +/-20 degrees, blue 240 +/-20, green 120 +/-20; saturation >=45%, value >=60/255.
static bool is_target_pixel(const uint8_t *p, TargetColor target) {
  int r=p[COLOR_INPUT_BGR ? 2 : 0],g=p[1],b=p[COLOR_INPUT_BGR ? 0 : 2];
  int hi=r>g?r:g; if(b>hi)hi=b;
  int lo=r<g?r:g; if(b<lo)lo=b;
  int d=hi-lo;
  if(hi<60 || d==0 || d*100<hi*45)return false;
  int hue_offset;
  if(target==TARGET_RED){if(hi!=r)return false;hue_offset=60*(g-b);}
  else if(target==TARGET_BLUE){if(hi!=b)return false;hue_offset=60*(r-g);}
  else if(target==TARGET_GREEN){if(hi!=g)return false;hue_offset=60*(b-r);}
  else if(target==TARGET_ORANGE){if(hi!=r)return false;hue_offset=60*(g-b);return hue_offset>20*d && hue_offset<=50*d;}
  else return false;
  return hue_offset>=-20*d && hue_offset<=20*d;
}
static bool is_red_pixel(const uint8_t *p){return is_target_pixel(p,TARGET_RED);}
static bool is_blue_pixel(const uint8_t *p){return is_target_pixel(p,TARGET_BLUE);}
static bool is_green_pixel(const uint8_t *p){return is_target_pixel(p,TARGET_GREEN);}
// Workspaces: n mask bytes and n uint32_t queue entries. Four-neighbour connectivity.
static ColorBlob find_color_blob(const uint8_t *rgb,int w,int h,uint8_t *mask,uint32_t *q,TargetColor target,int focusX=-1,int focusY=-1) {
  ColorBlob best={-1,-1,0,0,0,0,0};
  if(!rgb || !mask || !q || w<=0 || h<=0)return best;
  const uint32_t n=(uint32_t)w*h;
  for(uint32_t i=0;i<n;i++)mask[i]=is_target_pixel(rgb+3*i,target);
  for(uint32_t seed=0;seed<n;seed++) {
    if(!mask[seed])continue;
    uint32_t head=0,tail=0; uint64_t sx=0,sy=0;
    int x0=w,y0=h,x1=0,y1=0;
    mask[seed]=0;q[tail++]=seed;
    while(head<tail) {
      uint32_t i=q[head++];int x=i%w,y=i/w;
      sx+=x;sy+=y;
      if(x<x0)x0=x;if(x>x1)x1=x;if(y<y0)y0=y;if(y>y1)y1=y;
      uint32_t ns[4];int count=0;
      if(x>0)ns[count++]=i-1;if(x+1<w)ns[count++]=i+1;
      if(y>0)ns[count++]=i-w;if(y+1<h)ns[count++]=i+w;
      for(int k=0;k<count;k++)if(mask[ns[k]]) {mask[ns[k]]=0;q[tail++]=ns[k];}
    }
    bool nearFocus=focusX<0 || (focusX>=x0-12 && focusX<=x1+12 && focusY>=y0-12 && focusY<=y1+12);
    if(nearFocus && tail>=COLOR_MIN_PIXELS && tail>best.pixels) {
      best.x=sx/tail;best.y=sy/tail;best.x0=x0;best.y0=y0;best.x1=x1;best.y1=y1;best.pixels=tail;
    }
  }
  return best;
}
static ColorBlob find_red_blob(const uint8_t *rgb,int w,int h,uint8_t *mask,uint32_t *q) {
  return find_color_blob(rgb,w,h,mask,q,TARGET_RED);
}
static ColorBlob find_blue_blob(const uint8_t *rgb,int w,int h,uint8_t *mask,uint32_t *q) {
  return find_color_blob(rgb,w,h,mask,q,TARGET_BLUE);
}
static ColorBlob find_green_blob(const uint8_t *rgb,int w,int h,uint8_t *mask,uint32_t *q) {
  return find_color_blob(rgb,w,h,mask,q,TARGET_GREEN);
}
static void color_green_pixel(uint8_t *rgb,int w,int h,int x,int y) {
  if(x<0||y<0||x>=w||y>=h)return;
  uint8_t *p=rgb+3*(y*w+x);p[0]=0;p[1]=255;p[2]=0;
}
static void draw_color_blob(uint8_t *rgb,int w,int h,const ColorBlob &b) {
  if(!b.pixels)return;
  for(int x=b.x0;x<=b.x1;x++){color_green_pixel(rgb,w,h,x,b.y0);color_green_pixel(rgb,w,h,x,b.y1);}
  for(int y=b.y0;y<=b.y1;y++){color_green_pixel(rgb,w,h,b.x0,y);color_green_pixel(rgb,w,h,b.x1,y);}
  for(int d=-4;d<=4;d++){color_green_pixel(rgb,w,h,b.x+d,b.y);color_green_pixel(rgb,w,h,b.x,b.y+d);}
}
#endif
