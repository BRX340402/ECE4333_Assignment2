#pragma once
#include "esp_camera.h"
#include "esp_jpg_decode.h"
#include <string.h>
static const int ANALYSIS_MAX_W=400,ANALYSIS_MAX_H=400;
struct BoundedJpeg{const uint8_t *src;size_t len;uint8_t *dst;int w,h;};
static size_t readJpeg(void *arg,size_t index,uint8_t *buf,size_t len){auto*d=(BoundedJpeg*)arg;if(index>=d->len)return 0;if(len>d->len-index)len=d->len-index;if(buf)memcpy(buf,d->src+index,len);return len;}
static bool writeJpeg(void *arg,uint16_t x,uint16_t y,uint16_t w,uint16_t h,uint8_t *data){
 auto*d=(BoundedJpeg*)arg;if(!data){if(!x&&!y){if(w>ANALYSIS_MAX_W||h>ANALYSIS_MAX_H||!w||!h)return false;d->w=w;d->h=h;}return true;}
 if(x+w>d->w||y+h>d->h)return false;
 // Preserve the legacy fmt2rgb888 BGR memory order, without a full-size RGB allocation.
 for(int row=0;row<h;row++)for(int col=0;col<w;col++){uint8_t*p=d->dst+3*((y+row)*d->w+x+col);const uint8_t*q=data+3*(row*w+col);p[0]=q[2];p[1]=q[1];p[2]=q[0];}return true;
}
static bool decodeBounded(const camera_fb_t *fb,uint8_t *dst,int &w,int &h){
 int scale=0;while(scale<3 && ((fb->width>>scale)>ANALYSIS_MAX_W||(fb->height>>scale)>ANALYSIS_MAX_H))scale++;
 BoundedJpeg d={fb->buf,fb->len,dst,0,0};if(esp_jpg_decode(fb->len,(jpg_scale_t)scale,readJpeg,writeJpeg,&d)!=ESP_OK||!d.w||!d.h)return false;w=d.w;h=d.h;return true;
}
