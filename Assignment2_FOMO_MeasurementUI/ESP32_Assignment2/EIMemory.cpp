#include <Arduino.h>
#include <esp_heap_caps.h>
#include <string.h>
#include "edge-impulse-sdk/porting/ei_classifier_porting.h"
// Strong overrides for the SDK's weak allocators. The 154 KB arena must fit
// in one allocation; free internal heap alone is insufficient with Wi-Fi active.
void *ei_malloc(size_t size) {
  void *p=heap_caps_malloc(size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!p)p=heap_caps_malloc(size,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
  if(!p)Serial.printf("EI allocation failed: %u bytes\n",(unsigned)size);
  return p;
}
void *ei_calloc(size_t count,size_t size) {
  if(size && count>SIZE_MAX/size)return nullptr;
  size_t bytes=count*size;void *p=ei_malloc(bytes);
  if(p)memset(p,0,bytes);
  return p;
}
void ei_free(void *ptr) { heap_caps_free(ptr); }
