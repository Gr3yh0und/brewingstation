# Brewing Station (V1) — Breadboard

Automated beer brewing temperature controller, breadboard prototype. Reads temperature from multiple sensors, controls a GGM IDS2 induction cooker via a proprietary serial protocol, and runs a PID loop to hold a target mash temperature. Integrates with CraftBeerPi 3 and 4 via MQTT.

The idea: a "remote hands" station for CraftBeerPi with a local interface, so it also works standalone without a Pi — e.g. for manual-temperature-control outdoor cooking.

**MCU:** ESP32
**Toolchain:** Arduino IDE or PlatformIO (see [platformio.ini](platformio.ini))
**Custom-PCB variant:** [brewingstation2](../brewingstation2) — same firmware, fabricated 2-layer PCB instead of this breadboard
**Successor:** [brewingstation3](../brewingstation3) — full rewrite on ESP32-C6 with safety shutdowns, brew timer, and a printable case

---

## Induction Cooker

Within this project the [GGM Gastro IDS2 induction cooker](https://www.ggmgastro.com/de-de-eur/induktionsherd-3-5-kw-1) is used to heat the mash tun. It has an external control panel connected via a serial line — reverse-engineered by [InnuendoPi](https://github.com/InnuendoPi) in [MQTTDevice2](https://github.com/InnuendoPi/MQTTDevice2), which this project builds on.

Tested with both CraftBeerPi 3 and 4 — the MQTT API is identical between the two.

---

## Hardware

| Component | Interface | Purpose |
|---|---|---|
| DS18B20 (×2) | OneWire | Mash temperature |
| MAX31865 (PT1000) | Software SPI | High-accuracy RTD temperature |
| BME280 | Software SPI | Ambient temperature / humidity / pressure |
| GGM IDS2 induction cooker | 3-wire serial | Heat source |
| 2× SSD1306 OLED 128×64 | I2C, different addresses | Temperature + status display |
| Buttons + LEDs | GPIO | Manual power control (aluminium switches with integrated LEDs) |

---

## Design Files

| File | Description |
|---|---|
| `brewingstation.fzz` | Fritzing project |
| `brewingstation_breadboard.png` | Breadboard wiring view |
| `brewingstation_schematics.png` | Schematic / circuit diagram |
| `brewingstation_pcb.png` | PCB layout |
| `gerber_pcb.zip` | Gerber files for PCB fabrication |

---

## MQTT

Topics are built from `MQTT_ROOT_PATH / MQTT_DEVICE` (set in `config.h`).

| Topic | Direction | Payload fields |
|---|---|---|
| `.../temperature/N` | publish | `temperature` (°C) |
| `.../heater` | publish | `relayOn`, `inductionOn`, `powerPercent`, `powerLevel` |
| `.../heater/power` | subscribe | `state` (`"off"` or unset), `power` (0–100) |
| `.../pid` | publish | `state`, `P`, `I`, `D`, `input`, `targetTemperature`, `tempDiff`, `output` |
| `.../pid/#` | subscribe | PID tuning/setpoint commands |

---

## Configuration

Copy `config_example.h` to `config.h` and fill in your network credentials and server address. `config.h` is gitignored — never commit real credentials.

```cpp
#define HOSTNAME        "ESP-BREWING"
#define SSID_NAME       "your-ssid"
#define SSID_PASSWORD   "your-password"
#define SERVER_ADDRESS  "192.168.0.x"
#define MQTT_ROOT_PATH  "root"
#define MQTT_DEVICE     "brewery"
```

---

## Build & Flash

**PlatformIO:**

```sh
pio run -e esp32doit-devkit-v1
pio run -e esp32doit-devkit-v1 -t upload
pio device monitor
```

**Arduino IDE:** open `brewingstation.ino` directly and install the libraries listed at the top of the sketch.

---

## Known Gaps

This is the earliest surviving generation of the project — some functionality is missing or incomplete:

- PID integration (present in code, not fully tuned/validated)
- Sensor handling (no runtime detection/calibration — see [brewingstation3](../brewingstation3) for that)
- Humidity and pressure MQTT topics (BME280 reads them, but only temperature is published)
- Brew timers / mash-step display (also needs CraftBeerPi-side adaptations)

## Breadboard view

![breadboard](brewingstation_breadboard.png)

## Schematics

![schematics](brewingstation_schematics.png)

## Fritzing PCB / Gerber

![pcb](brewingstation_pcb.png)
