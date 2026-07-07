# Changelog

All notable changes to brewingstation3 (V3) are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added
- Extracted more hardware-independent logic into `pure_logic.h` for native unit
  testing: induction power→PWM-level math (`computePowerLevel`), the RX pulse
  classifier and frame decoder (`classifyPulse`/`decodeErrorCode`), the brew
  timer as a testable state machine (`BrewTimerState`), safety predicates
  (`sensorIsHealthy`/`isThermalRunaway`), and display row-1 formatting
  (`computeRow1Display`). 31 new tests (45 total). No behavior change.
- Settings persistence via LittleFS: power cap, device mode, and PID
  state/tunings/setpoint now survive a reboot instead of reverting to
  `config.h` defaults. Saved to `/settings.json` whenever changed via MQTT,
  reloaded at boot after `setup_pid()`.
- WiFiManager captive-portal provisioning, replacing the hardcoded
  `SSID_NAME`/`SSID_PASSWORD` + reboot-on-fail Wi-Fi connect. Opens
  automatically on first boot or a failed connect, or can be forced by
  holding any panel button for ~2s at power-on. The same portal now also
  provisions the MQTT broker address, MQTT topic prefix (root/device), and
  OTA password as runtime-configurable custom fields (persisted to
  `/netconfig.json`) — these no longer require a reflash to change. The 13
  MQTT subscribe-topic strings were converted from compile-time macros to
  runtime buffers to support this.
- Board's partition table switched from the stock 4MB scheme to
  `default_8MB.csv` to match the actual N8 (8MB flash) module — each OTA
  app slot grows from 1.25MB to ~3.19MB and LittleFS from 1.4MB to 1.5MB,
  needed to make room for WiFiManager and future web-dashboard work.
- Built-in web server (`WebServer`/`Update`, both bundled with the
  arduino-esp32 core — no new `lib_deps`) exposing three tabbed pages
  under a shared nav bar:
  - `/` — read-only status dashboard (sensors, setpoint, PID/mode,
    induction power/cap, relay, GPIO5, brew timer, RSSI, uptime, free
    heap, version), auto-refreshes every 5s.
  - `/update` — browser-based firmware upload form; streams straight
    into the inactive OTA partition via the `Update` library, then
    reboots.
  - `/config` — current runtime configuration (MQTT broker/topic
    prefix, WiFi SSID/IP, power cap, device mode, PID tunings/setpoint;
    OTA password masked), with a "Download config (.json)" link
    (`/config/download`) that exports the same data — including the
    plaintext OTA password — as a downloadable JSON file for backup.
  `/update` and `/config` (+ its download) are gated behind HTTP Basic
  Auth using the same `cfgOtaPassword` ArduinoOTA already uses; `/` is
  unauthenticated (read-only, no secrets). The shared nav bar shows the
  hostname and current firmware version. The `/update` page also links
  to the project's GitHub releases page and has a "Check for update"
  button that runs entirely in the browser (`fetch()` against the
  GitHub releases API, filtered to `3.*` tags, compared against the
  running version) — no TLS/HTTPS client added to the firmware itself.

## [3.0.0-alpha.2] - 2026-07-07

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
- Release notes are now sourced from this changelog instead of
  auto-generated commit/PR lists.

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

[Unreleased]: https://github.com/Gr3yh0und/brewingstation/compare/3.0.0-alpha.2...HEAD
[3.0.0-alpha.2]: https://github.com/Gr3yh0und/brewingstation/compare/3.0.0-alpha.1...3.0.0-alpha.2
[3.0.0-alpha.1]: https://github.com/Gr3yh0und/brewingstation/releases/tag/3.0.0-alpha.1
