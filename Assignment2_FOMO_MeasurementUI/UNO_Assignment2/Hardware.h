#pragma once
// User's ex 2 is TB6612: right AIN=D7, left BIN=D8, STBY=D3.
// Do NOT substitute the official older DRV8835 pin mapping.
#define MOTOR_RIGHT_PWM 5
#define MOTOR_LEFT_PWM 6
#define MOTOR_RIGHT_DIR 7
#define MOTOR_LEFT_DIR 8
#define MOTOR_STBY 3
#define SERVO_PAN_PIN 10
#define RGB_PIN 4
#define KEY_PIN 2
#define IR_PIN 9
#define US_TRIG_PIN 13
#define US_ECHO_PIN 12
#define LINE_LEFT_PIN A2
#define LINE_MIDDLE_PIN A1
#define LINE_RIGHT_PIN A0
#define BATTERY_PIN A3
// Tune using readings on your white floor and black tape, NOT guessed thresholds.
static const int LINE_THRESHOLD=500;
static const bool BLACK_IS_HIGH=true;
static const float BATTERY_SCALE=0.0375f; // official divider coefficient; verify with a meter
static const unsigned long REMOTE_TIMEOUT_MS=1800;
static const unsigned long AI_TIMEOUT_MS=2500;
static const int STOP_DISTANCE_CM=30;
static const int DRIVE_LIMIT=180;
