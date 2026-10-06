# Functional audit against supplied Assignment 1

Source: upload/Assignment1_Red_Blue_Green_Complete.zip, ESP32_CameraServer_AP_20220120/app_httpd.cpp, camera_index.h, CameraWebServer_AP.cpp, main .ino.
Hardware relationship: supplied Implementation principle of SmartRobot Car.pdf; UNO owns motors, ultrasonic/servo/floor sensor/LED. ESP32 owns camera/Wi-Fi. Original main ino has WiFiServer(100), RXD2=33, TXD2=4, Serial2 9600 and forwarding. Camera module FAQ p7 specifies core1.0.4.

| Original control group | FullUI 3 implementation |
|---|---|
| framesize / quality / brightness / contrast | Camera basic settings; /control setter + /status readback |
| saturation / special_effect / awb / awb_gain / wb_mode | White balance & colour |
| aec / aec2 / ae_level / aec_value / agc / agc_gain / gainceiling | Exposure & gain |
| bpc / wpc / raw_gma / lenc / dcw / colorbar | Sensor corrections |
| hmirror / vflip | Camera basic settings |
| face_detect / face_recognize / face_enroll | Detection > Assignment 1 face functions; original legacy SDK settings, five samples, seven RAM IDs |
| Get Still / stream start-stop / close image | Preview toolbar, with paired analysis preview and separate raw MJPEG |
| Red / blue / green detection | Colour regions toggle; orange retained from Assignment 2 |
| Wi-Fi AP / TCP100 / Serial2 bridge | Retained; optional UNO controls separated from camera |
| A2 calibration / photos / crop / telemetry / drive controls | Retained on separate pages, with resolution-aware calibration |

Intentional clarifications:
- Detection overlays use the same completed frame as the JPEG. No fake complete sign bounding box: FOMO is a location marker, measured red region is separate.
- Up to four FOMO STOP results, rather than discarding all but the highest confidence one.
- Original training centre crop is default and shown explicitly; full-frame letterbox and movable crops are selectable alternatives.
- Photo resolution and bounded analysis resolution are different. Full model remains96×96. High resolution alone cannot guarantee detection.
- Face IDs are not persistent, matching original RAM-only implementation.
- No new model training or hardware range sensor integration is claimed.
