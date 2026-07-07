// Pure, hardware-independent logic pulled out of main.cpp so it can be unit
// tested under the PlatformIO "native" environment (no Arduino/ESP32 headers).
#pragma once

#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

// Result of mapping a 0-100 power request onto the induction cooker's 6 discrete
// hardware steps (P0..P5): which command index to send, and how long (of the
// powerSampletime duty cycle window) to hold it at cmdIndex vs. cmdIndex-1.
struct PowerLevel {
  uint8_t cmdIndex;   // 0 = off, 1-5 = P1-P5
  long powerHigh;      // time (same unit as sampleTime) to hold cmdIndex
  long powerLow;       // time to hold cmdIndex-1 (0 if power lands exactly on a step)
};

// steps must be 6 ascending values with steps[0] == 0 (e.g. {0,20,40,60,80,100}).
inline PowerLevel computePowerLevel(int power, long sampleTime, const uint8_t steps[6]) {
  power = power < 0 ? 0 : (power > 100 ? 100 : power);
  if (power == 0) return PowerLevel{0, 0, 0};

  uint8_t cmdIndex = 5;
  long difference = 0;
  for (int i = 1; i < 6; i++) {
    if (power <= steps[i]) { cmdIndex = i; difference = steps[i] - power; break; }
  }

  long powerLow, powerHigh;
  if (difference != 0) {
    // Interpolate between steps[cmdIndex-1] and steps[cmdIndex]: spend
    // proportionally more time at the lower step the closer `power` is to it.
    powerLow  = sampleTime * difference / 20L;
    powerHigh = sampleTime - powerLow;
  } else {
    powerHigh = sampleTime;
    powerLow  = 0;
  }
  return PowerLevel{cmdIndex, powerHigh, powerLow};
}

// Induction cooker serial protocol pulse timing (microseconds), used both to
// build the outgoing command bit patterns and to classify incoming RX pulses.
constexpr long SIGNAL_HIGH      = 5120;
constexpr long SIGNAL_HIGH_TOL  = 1500;
constexpr long SIGNAL_LOW       = 1280;
constexpr long SIGNAL_LOW_TOL   = 500;
constexpr long FRAME_START_MIN_US = 15000;
constexpr long FRAME_START_MAX_US = 35000;

enum class PulseType { NOISE, START, BIT_ONE, BIT_ZERO, UNRECOGNIZED };

// Classifies a single falling-edge pulse duration from the induction cooker's RX
// line. Context-free by design (doesn't know whether a frame is in progress) so
// it's trivially testable; the caller (the ISR) still decides what a given
// classification means based on its own inputStarted/inputCurrent state.
inline PulseType classifyPulse(long signalTimeUs) {
  if (signalTimeUs <= 10) return PulseType::NOISE;
  if (signalTimeUs > FRAME_START_MIN_US && signalTimeUs < FRAME_START_MAX_US) return PulseType::START;
  if (signalTimeUs > (SIGNAL_HIGH - SIGNAL_HIGH_TOL) && signalTimeUs < (SIGNAL_HIGH + SIGNAL_HIGH_TOL)) return PulseType::BIT_ONE;
  if (signalTimeUs > (SIGNAL_LOW - SIGNAL_LOW_TOL) && signalTimeUs < (SIGNAL_LOW + SIGNAL_LOW_TOL)) return PulseType::BIT_ZERO;
  return PulseType::UNRECOGNIZED;
}

// Extracts the 4-bit error code from a decoded 33-bit induction cooker status
// frame (bits 13-16, MSB first) — see bug #1 in the code review: an off-by-one
// in the caller that filled this buffer once wrote past bits[32] into adjacent
// class state. A bounds-checked frame decoder here can't reproduce that class
// of bug even if the caller regresses.
inline uint8_t decodeErrorCode(const unsigned char bits[33]) {
  return bits[13] * 8 + bits[14] * 4 + bits[15] * 2 + bits[16];
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

// Single source of truth for the "slave"/"standalone" mode label — was previously
// duplicated inline as `deviceMode == MODE_SLAVE ? "slave" : "standalone"` at 5
// call sites (MQTT device status, OLED display, web dashboard/config page).
inline const char* deviceModeName(bool isSlave) {
  return isSlave ? "slave" : "standalone";
}

// True if the VERSION string identifies a pre-release build (alpha/beta), used by the
// web dashboard's nav bar to flag a non-release firmware with a warning badge.
inline bool isPrereleaseVersion(const char* version) {
  return strstr(version, "alpha") != nullptr || strstr(version, "beta") != nullptr;
}

// ─── Brew timer state machine ──────────────────────────────────────────────────
// millis()-based, with `now` injected by the caller so it's testable (including
// wraparound, via deadlineReached()) without depending on the real clock.

struct BrewTimerState {
  uint32_t durationMs      = 0;
  uint32_t endMs           = 0;
  uint32_t pauseRemainingMs = 0;
  bool running = false;
  bool paused  = false;
};

// timer/set — arms a new duration; cancels any running/paused timer.
inline void brewTimerSet(BrewTimerState &s, uint32_t durationMs) {
  s.durationMs = durationMs;
  s.running = s.paused = false;
  s.endMs = s.pauseRemainingMs = 0;
}

inline uint32_t brewTimerRemainingMs(const BrewTimerState &s, uint32_t now) {
  if (s.paused)  return s.pauseRemainingMs;
  if (s.running) return deadlineReached(now, s.endMs) ? 0 : s.endMs - now;
  return 0;
}

// timer/ctl {cmd:"start"} — starts fresh, or resumes from a pause. No-op if no
// duration has been set yet.
inline void brewTimerStart(BrewTimerState &s, uint32_t now) {
  if (s.durationMs == 0) return;
  uint32_t remaining = s.paused ? s.pauseRemainingMs : s.durationMs;
  s.endMs   = now + remaining;
  s.running = true;
  s.paused  = false;
}

// timer/ctl {cmd:"pause"} — no-op if not currently running.
inline void brewTimerPause(BrewTimerState &s, uint32_t now) {
  if (!s.running) return;
  s.pauseRemainingMs = brewTimerRemainingMs(s, now);
  s.running = false;
  s.paused  = true;
}

// timer/ctl {cmd:"reset"} — clears everything, including the armed duration.
inline void brewTimerReset(BrewTimerState &s) {
  s.durationMs = 0;
  s.running = s.paused = false;
  s.endMs = s.pauseRemainingMs = 0;
}

// Called every loop() iteration (not just on display refresh — see judgment-call
// fix in this session) so expiry can't be delayed by the display's refresh
// cadence. Returns true exactly once, on the tick where expiry is detected, so
// the caller knows to fire the buzzer/log exactly once.
inline bool brewTimerCheckExpiry(BrewTimerState &s, uint32_t now) {
  if (s.running && brewTimerRemainingMs(s, now) == 0) {
    s.running = false;
    return true;
  }
  return false;
}

// ─── Safety predicates ──────────────────────────────────────────────────────────
// Pure functions of (now, state) rather than reading globals directly, so the
// exact conditions that trigger a safety shutdown are independently testable.

inline bool sensorIsHealthy(uint32_t now, uint32_t lastReadMs, uint32_t staleTimeoutMs, int deviceCount) {
  return deviceCount > 0 && !deadlineReached(now, lastReadMs + staleTimeoutMs);
}

inline bool isThermalRunaway(double input, double setpoint, double overshootThreshold) {
  return setpoint > 0.0 && input > setpoint + overshootThreshold;
}

// ─── Display row 1 (timer / clock / uptime) ─────────────────────────────────────
// Priority: brew timer (running or paused) > NTP clock (once synced) > uptime.
// label must be >= 6 bytes, value >= 9 bytes (matches main.cpp's buffer sizes).

inline void computeRow1Display(bool timerRunning, bool timerPaused, uint32_t timerRemainingMs,
                                bool ntpSynced, int hour, int minute, uint32_t uptimeMs,
                                char label[6], char value[9]) {
  if (timerRunning || timerPaused) {
    strcpy(label, "Tmr");
    sprintf(value, "%02lu:%02lu", (unsigned long)(timerRemainingMs / 60000UL), (unsigned long)((timerRemainingMs / 1000UL) % 60UL));
  } else if (ntpSynced) {
    strcpy(label, "Clk");
    sprintf(value, "%02d:%02d", hour, minute);
  } else {
    strcpy(label, "Run");
    sprintf(value, "%03lu:%02lu", (unsigned long)(uptimeMs / 60000UL), (unsigned long)((uptimeMs / 1000UL) % 60UL));
  }
}
