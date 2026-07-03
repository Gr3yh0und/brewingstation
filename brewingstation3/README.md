# Brewing Station 3

Automated beer brewing temperature controller. Reads temperature from multiple sensors, controls a GGM IDS2 induction cooker via a proprietary serial protocol, and runs a PID loop to hold a target mash temperature. Integrates with CraftBeerPi 3 and 4 via MQTT.

**Status: work in progress.** Firmware compiles but is not yet proven on real hardware — the PCB hasn't been soldered/assembled, nothing has been tested end-to-end, and the 3D-printable case hasn't been printed.

**Board:** ESP32-C6-WROOM-1U (ESP32-C6-DevKitC-1-N8)  
**Toolchain:** PlatformIO + VS Code (see [platformio.ini](platformio.ini))  
**Schematic:** KiCad 10 — see [kicad/](kicad/)

| Document | Contents |
|---|---|
| [HARDWARE.md](HARDWARE.md) | Sensors, actuators, connectors, pin map, IDS2 protocol, displays |
| [BOM.md](BOM.md) | Full bill of materials with order links |
| [CASE.md](CASE.md) | 3D-printable enclosure — dimensions, assembly, panel layout |

---

## Software Features

### Brewing Control
- **PID temperature control** — Proportional on Measurement (P_ON_M) mode. Default tuning: P=45.27, I=0.2371, D=7.2. All parameters adjustable at runtime via MQTT. Setpoint can be configured while PID is off.
- **Induction cooker control** — GGM IDS2 controlled via a proprietary 33-bit pulse-width serial protocol. 6 discrete power levels (P0–P5); intermediate percentages achieved by PWM-cycling between adjacent levels in a 20-second window. Relay stays on for 60 seconds after power-off for fan cooldown.
- **Multi-sensor support** — Up to 5 sensors total: DS18B20 (OneWire, auto-detected), PT100X via MAX31865, BME680. Sensors auto-detected at boot; bad DS18B20 readings (-127°C) are discarded and the previous value retained.
- **Power cap** — Configurable maximum induction output percentage (`INDUCTION_POWER_CAP`, default 100%). Applied to both the PID output path and direct `heater/power` MQTT commands. Adjustable at runtime via `induction/cap` MQTT topic without reflashing.
- **Brew timer** — Countdown timer driven via MQTT (`timer/set`, `timer/control`). Supports start, pause, and reset. Remaining time shown on Display 1 when active. Expiry logged via syslog.
- **Safety shutdowns** — Three automatic cutoffs disable induction power and the PID: (1) sensor staleness: primary sensor has not reported for 30 seconds; (2) thermal runaway: temperature exceeds setpoint by more than 5°C; (3) cooker error codes: E3 overheat, E7 low voltage, or E8 high voltage received from the cooker RX line. All events are logged via syslog at CRIT severity.

### User Interface
- **Dual OLED display** — Display 1: runtime/clock/timer, PID target temperature, PID output %, Wi-Fi/MQTT status icons. Display 2: sensor temperatures, induction power level and percentage, fan indicator, PID active flag.
- **Button control** — 6 panel buttons on a single ADC pin (IO1) via resistor ladder. Each button directly sets the GGM IDS2 induction cooker to a fixed power level (0 / 20 / 40 / 60 / 80 / 100%). Debounced (250 ms). See button mapping table below.
- **Power-level LEDs** — 8 LEDs driven via PCF8574 I2C GPIO expander (address 0x20), active-LOW. The lit LED always reflects the current IDS2 power state: whichever power level is active, the corresponding LED turns on. The LED updates whenever the power level changes — whether set by a button press, MQTT command, or PID output. P6/P7 are spare outputs available for custom use.

### Network & Platform
- **CraftBeerPi 3 & 4** compatible via MQTT.
- **OTA firmware updates** — ArduinoOTA on port 8266, password-protected (`OTA_PASSWORD` in `config.h`).
- **mDNS** — Device advertises as `ESP-BREWING.local`.
- **Syslog** — UDP logging to backend server on port 514.
- **NTP time sync** — `configTzTime()` called after WiFi connects; POSIX timezone string configurable via `NTP_TIMEZONE` in `config.h`. When synced, Display 1 shows wall-clock time instead of uptime.
- **Hardware watchdog** — ESP32 task watchdog timer (`WDT_TIMEOUT_S`, default 30 s). Reboots the device if `loop()` stalls for any reason.

### Button Mapping

Each button sets the GGM IDS2 induction cooker to a fixed power level. The matching power-level LED lights up immediately after the button is pressed. See [HARDWARE.md](HARDWARE.md#button--led-panel-external--not-on-pcb) for wiring details.

| Button | Panel ref | ADC range | IDS2 power | LED |
|---|---|---|---|---|
| B1 | SW2 | 3200–3299 | 0% (off) | LD1 (LED_0) |
| B2 | SW3 | 3000–3199 | 20% | LD2 (LED_20) |
| B3 | SW4 | 2600–2999 | 40% | LD3 (LED_40) |
| B4 | SW5 | 2000–2599 | 60% | LD4 (LED_60) |
| B5 | SW6 | 1–1999 | 80% | LD5 (LED_80) |
| B6 | SW7 | 0 (ADC_BTN shorted to GND) | 100% | LD6 (LED_100) |

> The LEDs also update when the power level is changed via MQTT or PID output — the lit LED always reflects the **current** IDS2 state, not just the last button pressed.

---

## MQTT

All topics are built from `MQTT_ROOT_PATH / MQTT_DEVICE` (set in `config.h`).

| Topic | Direction | Payload fields |
|---|---|---|
| `.../sensor/N` | publish | `temp` (°C), `ts` |
| `.../induction` | publish | `relay`, `active`, `power`, `level`, `error`, `cap`, `ts` |
| `.../induction/set` | subscribe | `power` (0–100); `state "off"` cuts power |
| `.../induction/cap` | subscribe | `cap` (0–100) — runtime power ceiling |
| `.../pid` | publish | `enabled`, `mode`, `setpoint`, `input`, `output`, `diff`, `P`, `I`, `D`, `ts` |
| `.../pid/enable` | subscribe | `enabled` (bool) |
| `.../pid/reset` | subscribe | _(any payload)_ — resets PID output to 0 |
| `.../pid/p` | subscribe | `P` (float) |
| `.../pid/i` | subscribe | `I` (float) |
| `.../pid/d` | subscribe | `D` (float) |
| `.../pid/setpoint` | subscribe | `setpoint` (float, 0–100°C) |
| `.../timer` | publish | `running`, `paused`, `remaining` (s), `duration` (s), `ts` |
| `.../timer/set` | subscribe | `duration` (seconds) |
| `.../timer/ctl` | subscribe | `cmd`: `"start"` / `"pause"` / `"reset"` |
| `.../device` | publish | `uptime`, `heap`, `rssi`, `version`, `mode`, `ts` |
| `.../device/mode` | subscribe | `mode`: `"standalone"` / `"slave"` |
| `.../relay` | publish | `state`, `remaining`, `ts` |
| `.../relay/set` | subscribe | `state`: `"on"` / `"off"`; optional `duration` (s) |
| `.../gpio_ext` | publish | `state`, `remaining`, `ts` |
| `.../gpio_ext/set` | subscribe | `state`: `"on"` / `"off"`; optional `duration` (s) |

---

## Configuration

Copy `include/config_example.h` to `include/config.h` and fill in your values. `config.h` is gitignored.

```cpp
#define HOSTNAME        "ESP-BREWING"
#define SSID_NAME       "your-ssid"
#define SSID_PASSWORD   "your-password"
#define SERVER_ADDRESS  "192.168.0.x"   // Raspberry Pi IP
#define MQTT_ROOT_PATH  "cave"
#define MQTT_DEVICE     "brewery"
```

**PT100X vs PT1000:** Default is PT1000 (`R_NOM=1000`, `R_REF=4300`). Change both to `100` / `430` for PT100.

**PID tuning:** Default values (`P=45.27`, `I=0.2371`, `D=7.2`) are tuned for a specific vessel. Adjust `PID_DEFAULT_P/I/D` in `config.h`. Values can also be changed at runtime via MQTT.

**Button ADC thresholds:** `BUTTON_THRESHOLD_B1–B5` depend on actual resistor values. Measure with serial monitor and adjust if buttons trigger at the wrong levels.

**Safety thresholds:** `PID_SAFETY_OVERSHOOT` (default 5°C) and `SENSOR_STALE_TIMEOUT_MS` (default 30s).

**Power cap:** `INDUCTION_POWER_CAP` (default 100%). Override at runtime via `induction/cap` MQTT.

**Watchdog:** `WDT_TIMEOUT_S` (default 30 s).

**NTP:** `NTP_SERVER` (default `pool.ntp.org`) and `NTP_TIMEZONE` (POSIX string). See [nayarsystems/posix_tz_db](https://github.com/nayarsystems/posix_tz_db).

---

## Build & Flash (PlatformIO)

```sh
# Build
pio run -e esp32c6

# Upload via USB-CDC (no external USB-UART chip needed)
pio run -e esp32c6 -t upload

# Serial monitor
pio device monitor

# Debug build (verbose ESP-IDF logs)
pio run -e esp32c6_debug
```

Or use the PlatformIO sidebar in VS Code (Build ✓ / Upload → / Monitor icons).

---

## Libraries

All dependencies are managed by PlatformIO via `platformio.ini`.

| Library | Author | Constraint | Installed |
|---|---|---|---|
| PubSubClient | Nick O'Leary | `^2.8` | 2.8 |
| ArduinoJson | Benoit Blanchon | `^7.0` | 7.4.3 |
| Syslog | Martin Sloup (arcao) | `^2.0` | 2.0.0 |
| Arduino-PID-Library | Brett Beauregard | `^1.2` | 1.2.1 |
| OneWire | Paul Stoffregen | local `lib/` | 2.3.8 (patched) |
| DallasTemperature | Miles Burton | `^3.9` | 3.11.0 |
| Adafruit Unified Sensor | Adafruit | `^1.1` | 1.1.15 |
| Adafruit MAX31865 | Adafruit | `^1.1` | 1.6.2 |
| Adafruit BME680 | Adafruit | `^2.2` | 2.3.0 |
| Adafruit GFX | Adafruit | `^1.11` | 1.12.6 |
| Adafruit SSD1306 | Adafruit | `^2.5` | 2.5.16 |
| PCF8574 | Rob Tillaart | `^0.3` | 0.3.9 |
| arduino-timer | contrem | unpinned | 3.0.1 |

**OneWire note:** Kept as a local copy in `lib/OneWire/` because the upstream release does not handle the ESP32-C6 GPIO register struct (typed fields requiring `.val` in arduino-esp32 3.x). The patch adds `|| CONFIG_IDF_TARGET_ESP32C6` to each `#if CONFIG_IDF_TARGET_ESP32C3` guard in `util/OneWire_direct_gpio.h`.

**Platform:** pioarduino community fork (`pioarduino/platform-espressif32 @ stable`). The official `espressif32` platform references a RISC-V toolchain package (`toolchain-riscv32-esp @ 13.2.0+20240530`) that was removed from the PlatformIO registry; the pioarduino fork bundles the toolchain directly. Bundles arduino-esp32 3.x, which is required for ESP32-C6 support.
