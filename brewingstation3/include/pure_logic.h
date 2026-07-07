// Pure, hardware-independent logic pulled out of main.cpp so it can be unit
// tested under the PlatformIO "native" environment (no Arduino/ESP32 headers).
#pragma once

#include <stdint.h>

// Derives slope/offset from two (raw, reference) calibration points: corrected = raw * slope + offset
inline void computeCalibration(float raw1, float ref1, float raw2, float ref2, float &slope, float &offset) {
  slope = (ref2 - ref1) / (raw2 - raw1);
  offset = ref1 - slope * raw1;
}

// Maps a button-ladder ADC reading to a bucket (0 = no press, 1-6 = B1-B6),
// given the five threshold boundaries (see BUTTON_THRESHOLD_B1..B5 in config.h).
inline int getButtonBucket(int val, int thresholdB1, int thresholdB2, int thresholdB3,
                            int thresholdB4, int thresholdB5) {
  if (val == 0)               return 6;  // B6: 100%
  if (val < thresholdB5)      return 5;  // B5:  80%
  if (val < thresholdB4)      return 4;  // B4:  60%
  if (val < thresholdB3)      return 3;  // B3:  40%
  if (val < thresholdB2)      return 2;  // B2:  20%
  if (val < thresholdB1)      return 1;  // B1:   0%
  return 0;                              // no press
}

// Overflow-safe "has this millis()-based deadline been reached" check. A plain
// `now >= deadline` comparison breaks once `now` wraps past `deadline` (every ~49.7
// days on a 32-bit millis() counter); subtracting first and casting to signed handles
// the wraparound correctly as long as the actual elapsed time is under ~24 days.
//
// Fixed-width uint32_t (not unsigned long) is deliberate: millis() returns a 32-bit
// value on the real ESP32 target, but `unsigned long` is 64-bit on the native/host test
// platform, so a `long`/`unsigned long` version here would never actually wrap when
// unit-tested — it'd silently test different arithmetic than what runs on-device.
inline bool deadlineReached(uint32_t now, uint32_t deadline) {
  return (int32_t)(now - deadline) >= 0;
}

inline const char* inductionErrorString(uint8_t code) {
  switch (code) {
    case 1: case 2: return "E0:NoPot";
    case 3:         return "E1:Circ";
    case 4: case 5: return "E3:OHeat";
    case 6:         return "E4:Sens";
    case 9:         return "E7:LoVlt";
    case 10:        return "E8:HiVlt";
    case 14:        return "EC:Panel";
    default:        return "Err:???";
  }
}
