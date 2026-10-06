# Measurement UI validation
Full ESP32 compile and link PASS, Arduino ESP32 core 1.0.4, Xtensa GCC 5.2.0.
FQBN: esp32:esp32:esp32:PSRAM=enabled,PartitionScheme=huge_app,FlashSize=4M
Sketch: 1005726 bytes (31%); static globals: 49952 bytes (15%).
Model: supplied A2_StopSign_FOMO_104 1.0.4, weights unchanged.
Host C++ test PASS: red region association with a larger distant distractor, uncalibrated distance, calibrated distance scaling, clipped-region rejection, FOMO-only size rejection, per-colour calibration isolation.
JavaScript syntax PASS. Mock DOM interaction PASS: measurement table, sampled-width POST payload, refusal to calibrate FOMO-only location. No browser screenshot verification.
UNO source and model archive byte-identical to RuntimeFix.
No physical camera, NVS persistence, distance accuracy, or motor behaviour test performed.
