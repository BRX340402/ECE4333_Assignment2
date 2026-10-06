#include "Preview.h"
#include "freertos/semphr.h"
#include <string.h>
static SemaphoreHandle_t mutex;
static uint8_t *jpeg=nullptr;
static size_t jpegLength=0;
static uint16_t jpegSeq=0;
bool previewInit(){mutex=xSemaphoreCreateMutex();return mutex!=nullptr;}
bool previewStore(const camera_fb_t *fb,uint16_t seq){
 if(!fb||fb->format!=PIXFORMAT_JPEG||!fb->len)return false;
 uint8_t *next=(uint8_t*)ps_malloc(fb->len);if(!next)return false;
 memcpy(next,fb->buf,fb->len);
 xSemaphoreTake(mutex,portMAX_DELAY);
 uint8_t *old=jpeg;jpeg=next;jpegLength=fb->len;jpegSeq=seq;
 xSemaphoreGive(mutex);free(old);return true;
}
bool previewCopy(uint8_t *&data,size_t &length,uint16_t &seq){
 data=nullptr;length=0;seq=0;
 if(xSemaphoreTake(mutex,pdMS_TO_TICKS(20))!=pdTRUE)return false;
 if(jpeg&&jpegLength){data=(uint8_t*)ps_malloc(jpegLength);
  if(data){length=jpegLength;seq=jpegSeq;memcpy(data,jpeg,length);}}
 xSemaphoreGive(mutex);return data!=nullptr;
}
