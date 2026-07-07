# Changelog

All notable changes to brewingstation3 (V3) are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Fixed
- 8 bugs from a full code review: out-of-bounds write in
  `induction::readInput()`, `pid/reset` not actually resetting PID_v1's
  internal state, unvalidated MQTT JSON payload types, button power-cap
  bypass, `millis()` 32-bit wraparound handling (new `deadlineReached()`
  helper, used at all 8 timing sites), missing `IRAM_ATTR` on an ISR,
  an ISR race in `setup_induction()`, and a sensor-staleness blind spot
  for PID sensors other than DS18B20.
- 4 judgment-call issues from the same review: fragile float-equality
  sensor fault detection (replaced with a threshold), timer-expiry check
  decoupled from the display refresh rate, redundant self-subscription
  removed, and `pid/enable` no longer accepted while in slave mode.
- Regenerated `include/config.h` pin layout, which had drifted to an
  earlier "custom carrier PCB" board revision with two dangerous pin
  choices (a boot-strapping pin used for the relay, a non-exposed pin
  for the spare GPIO).

### Added
- OTA update workflow and a full 10-stage post-assembly bring-up checklist
  (power-on, sensors, induction cooker, buttons/LEDs, PID tuning, safety
  systems, buzzer, timer/MQTT control, case fit, soak test) to the backlog.

### Removed
- Stray tracked KiCad tooling backup files (`.mcp-backups/`,
  `_restore_backup_*/`, `kicad.zip`).

## [3.0.0-alpha.1] - 2026-07-03

First tagged pre-release. Firmware compiles and passes its unit tests, but
the hardware has not been assembled yet (PCB unsoldered, case unprinted,
nothing tested end-to-end).

### Added
- Buzzer firmware (LEDC PWM tone/alarm for step-complete and safety alarms).
- Two-point linear calibration for DS18B20, MAX31865, and BME680 sensors.
- Native unit tests (PlatformIO `native` environment) covering calibration,
  button-bucket mapping, and induction error-code logic.
- Versioning/release workflow: `3.*` tag scheme (kept separate from V1's
  plain `vX.Y.Z` tags), published as a GitHub prerelease.

### Changed
- Firmware explicitly marked as work-in-progress (`VERSION` string,
  README/CLAUDE.md notes) to signal untested hardware status.

[Unreleased]: https://github.com/Gr3yh0und/brewingstation/compare/3.0.0-alpha.1...HEAD
[3.0.0-alpha.1]: https://github.com/Gr3yh0und/brewingstation/releases/tag/3.0.0-alpha.1
