#pragma once
#define A2_ENABLE_AI 1
#define A2_MODEL_HEADER "A2_StopSign_FOMO_104.h"
#define COLOR_INPUT_BGR 1 // legacy decoder order; verify with a red/blue object
#define CAMERA_MODEL_M5STACK_WIDE
static const char *AP_SSID = "ECE4333_A2_V2";
static const char *AP_PASSWORD = "12345678";
static const int AP_CHANNEL = 9;
static const int UART_RX = 33, UART_TX = 4;
static const int IMAGE_W = 320, IMAGE_H = 240;
static const float AI_THRESHOLD = 0.80f;
static const uint32_t FRAME_INTERVAL_MS = 350;
// Calibration product K = known distance(cm) * measured coloured width(px).
// Zero means NOT calibrated: distance is -1. Measure separately for each physical object.
static const float STOP_K = 0.0f;
static const float RED_LIGHT_K = 0.0f;
static const float GREEN_LIGHT_K = 0.0f;
static const float CONE_K = 0.0f;
static const int BLACK_MAX = 65; // bottom 1/4 image, must be tuned to actual floor
