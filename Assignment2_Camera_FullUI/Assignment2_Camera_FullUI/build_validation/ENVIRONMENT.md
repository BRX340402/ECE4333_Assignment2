# FullUI 3 validation
Full compile/link PASS: Arduino ESP32 core 1.0.4, Xtensa GCC5.2.0.
FQBN esp32:esp32:esp32:PSRAM=enabled,PartitionScheme=huge_app,FlashSize=4M.
Final sketch2348418 bytes (74% of3145728); static globals78784 bytes (24%).
Includes legacy face detection/recognition library and original FOMO model simultaneously.

Host C++ geometry tests PASS: all nine camera resolution analysis sizes, full-frame letterbox bounds, centre crop, movable2x/4x crops at edges, red association against a larger distant distractor.
Bounded JPEG callback tests PASS: truncated reads, block boundary rejection, BGR byte order and output guard bytes. These test our callbacks, not the physical sensor or ESP32 ROM decoder.
Real headless Chromium UI tests PASS using mocked camera/UNO responses: original camera control IDs, resolution request, ROI controls, paired-frame display, calibration payload, rejection of FOMO-only size, UNO telemetry gating, mobile390px layout without page overflow.
Desktop/mobile screenshots inspected. Mock scores/images are not hardware recognition evidence.
UNO code and model archive byte-identical to MeasurementUI release.
No physical ESP32/UNO tests, no measured accuracy/range claims, no flash upload performed.
