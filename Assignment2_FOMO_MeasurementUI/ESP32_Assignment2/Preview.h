#pragma once
#include <Arduino.h>
#include "esp_camera.h"
bool previewInit();
bool previewStore(const camera_fb_t *fb,uint16_t seq);
bool previewCopy(uint8_t *&data,size_t &length,uint16_t &seq);
