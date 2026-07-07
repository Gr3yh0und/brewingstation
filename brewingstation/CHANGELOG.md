# Changelog

All notable changes to brewingstation (V1) are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

## [1.13.0] - 2026-07-03

Initial tagged release. One firmware, byte-identical across both hardware
variants (breadboard and custom PCB).

### Added
- PlatformIO support (`esp32doit-devkit-v1` environment) alongside the
  existing Arduino IDE compatibility.
- `hardware/breadboard/` and `hardware/pcb/` variant folders, merging
  brewingstation2 into brewingstation as a hardware variant of the same
  firmware rather than a separate maintained copy.
- README, `.gitignore`, and VS Code tooling brought up to brewingstation3's
  documentation/tooling conventions.

### Fixed
- Hardcoded `"cave/brewery"` MQTT topic strings replaced with topics built
  from the configurable `MQTT_ROOT_PATH`/`MQTT_DEVICE` values.
- `strcmp(topic, TOPIC_PID_ROOT) == 1` typo in the debug-build `mqttCallback`
  (should be `== 0`), which silently broke PID-topic matching in debug
  builds.

[Unreleased]: https://github.com/Gr3yh0und/brewingstation/compare/v1.13.0...HEAD
[1.13.0]: https://github.com/Gr3yh0und/brewingstation/releases/tag/v1.13.0
