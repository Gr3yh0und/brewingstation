# Brewing Station

Firmware, hardware, and PCB design for a DIY automated beer brewing controller. An ESP32-based device reads mash temperature from multiple sensors, drives a [GGM IDS2 induction cooktop](https://www.ggmgastro.com/de-de-eur/induktionsherd-3-5-kw-1) through PID control, and integrates with [CraftBeerPi](https://web.craftbeerpi.com/) via MQTT.

This repo tracks the project's evolution across several hardware/firmware generations.

Credit: the GGM IDS2 induction cooktop's RS232/serial protocol — pulse timings, command frames, power levels, and error codes — was reverse-engineered by [InnuendoPi](https://github.com/InnuendoPi) in [MQTTDevice2](https://github.com/InnuendoPi/MQTTDevice2). Every generation in this repo builds on that work.

## Own projects (in order of development)

| Folder | Status | Board | Build system | Notes |
|---|---|---|---|---|
| [brewingstation_old](brewingstation_old/) | Earliest snapshot | ESP8266 (Wemos D1 mini) | Arduino IDE (`.ino`) | Bare firmware only, no README/config split. First working version, later ported to ESP32. |
| [brewingstation](brewingstation/) | V1 | ESP32 | Arduino IDE (`.ino`) | Breadboard prototype. Documented in its own [README](brewingstation/README.md), includes Fritzing schematic and gerbers. |
| [brewingstation2](brewingstation2/) | V1, custom PCB | DOIT ESP32 DEVKIT V1 | Arduino IDE (`.ino`) | Same firmware as `brewingstation` (v1.13), fabricated 2-layer PCB instead of a breadboard. See [README](brewingstation2/README.md). |
| [brewingstation3](brewingstation3/) | **Active** | ESP32-C6-WROOM-1U | PlatformIO | Full rewrite: PID mash control, multi-sensor support, safety shutdowns, brew timer, KiCad schematic, 3D-printable case. See [README](brewingstation3/README.md), [HARDWARE.md](brewingstation3/HARDWARE.md), [BOM.md](brewingstation3/BOM.md), [CASE.md](brewingstation3/CASE.md). |

All generations share the same core idea: control the GGM IDS2 induction cooktop over its proprietary serial protocol, read temperature from DS18B20/PT100/PT1000/BME sensors, drive a couple of OLED displays and status LEDs, and talk to CraftBeerPi over MQTT.

`brewingstation3` is where active development happens; the earlier folders are kept as historical/reference hardware revisions.

## Getting started

Each own-project folder is self-contained with its own README and config. In general:

1. Pick a generation — `brewingstation3` for new builds, the earlier folders for reference or repairing existing hardware.
2. Copy `config_example.h` (or `include/config_example.h` for `brewingstation3`) to `config.h` and fill in your WiFi/MQTT/OTA settings. `config.h` is gitignored — never commit real credentials.
3. Follow the build instructions in that folder's README (Arduino IDE for `brewingstation`/`brewingstation2`/`brewingstation_old`, PlatformIO for `brewingstation3`).
