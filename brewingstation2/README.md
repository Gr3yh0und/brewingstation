# Brewing Station 2 — Custom PCB Edition

This is the custom PCB variant of the [Brewing Station](../brewingstation). The firmware is identical (v1.13, April 2023) — the difference is entirely in the hardware: a fabricated 2-layer PCB replaces the breadboard prototype.

## What it does

Automated beer brewing temperature controller. Reads temperature from multiple sensors, controls a GGM IDS2 induction cooker via a proprietary serial protocol, and runs a PID loop to hold a target mash temperature. Integrates with CraftBeerPi 3 and 4 via MQTT.

See the [parent project README](../brewingstation) and [repository CONTEXT](../CONTEXT.md) for full firmware documentation, MQTT topics, and the induction cooker protocol.

---

## Hardware

**MCU:** DOIT ESP32 DEVKIT V1

### Sensors
| Sensor | Interface | Purpose |
|---|---|---|
| DS18B20 (×3) | OneWire (GPIO16) | Mash temperature |
| MAX31865 / PT100X | Software SPI | High-accuracy RTD temperature |
| BME280 | Software SPI | Ambient temperature reference |

### Actuators & UI
| Component | Interface | Purpose |
|---|---|---|
| GGM IDS2 induction cooker | 3-wire serial (GPIO21/32/33) | Heat source |
| Relay | GPIO (via level shifter) | Cooker mains power switch |
| 6× LEDs (PL0–PL5) | GPIO 13/12/14/27/26/25 | Power level indicator (0/20/40/60/80/100%) |
| 6× buttons | Resistor ladder (GPIO34, analog) | Manual power level control |
| 2× SSD1306 OLED 128×64 | I2C 0x3C / 0x3D (GPIO15/2) | Temperature + status display |

### Additional PCB connectors
| Connector | Purpose |
|---|---|
| Relay | General-purpose relay output — e.g. pump or stirrer (Rührwerk) |
| PJump | Power source selector — jumper between PExt (external supply) and IDS2 supply |
| PExt | External power input connector |
| GPIOs | General-purpose GPIO header |

### Notable: Level Shifter
The schematic includes a logic level converter (3.3V ↔ 5V). This bridges the ESP32's 3.3V GPIO levels to the 5V signalling required by the induction cooker serial line and/or relay module.

---

## PCB Design Files

| File | Description |
|---|---|
| `brewingstation2.fzz` | Fritzing project (original) |
| `brewingstation2_updated.fzz` | Fritzing project (revised PCB layout) |
| `brewingstation2_Steckplatine.png` | Breadboard wiring view |
| `brewingstation2_Schaltplan.png` | Schematic / circuit diagram |
| `brewingstation2_Leiterplatte.png` | PCB layout (2-layer, "BREWSTATION" silkscreen) |
| `gerber.zip` | Gerber files for PCB fabrication |

The `_updated.fzz` reflects at least one PCB revision after initial design. Use the gerbers from the latest revision for fabrication.

---

## Pin Map (from `config_example.h`)

| Signal | GPIO |
|---|---|
| DS18B20 OneWire bus | 16 |
| PT100X CS | 22 |
| PT100X / BME280 DI (MOSI) | 4 |
| PT100X / BME280 DO (MISO) | 19 |
| PT100X / BME280 CLK | 5 |
| BME280 CS | 23 |
| I2C SDA (displays) | 15 |
| I2C SCL (displays) | 2 |
| Induction relay (WHITE) | 32 |
| Induction TX (YELLOW) | 33 |
| Induction RX interrupt (BLUE) | 21 |
| Button ladder (analog) | 34 |
| LED 0% | 13 |
| LED 20% | 12 |
| LED 40% | 14 |
| LED 60% | 27 |
| LED 80% | 26 |
| LED 100% | 25 |

---

## Configuration

Copy `config_example.h` to `config.h` and fill in your network credentials and server address. The `config.h` file is gitignored.

```cpp
#define HOSTNAME        "ESP-BREWING"
#define SSID_NAME       "your-ssid"
#define SSID_PASSWORD   "your-password"
#define SERVER_ADDRESS  "192.168.0.x"   // Raspberry Pi IP
#define MQTT_ROOT_PATH  "cave"
#define MQTT_DEVICE     "brewery"
```

---

## Differences vs `brewingstation` (breadboard)

| Aspect | brewingstation | brewingstation2 |
|---|---|---|
| Hardware form | Breadboard | Custom fabricated PCB |
| Level shifter | Not shown | Present (3.3V ↔ 5V) |
| DS18B20 count | 2 (config default) | 3 (shown in schematic) |
| Expansion connectors | None | PPump, PJump, PExt, GPIOs |
| Fritzing revisions | 1 | 2 (`_updated.fzz`) |
| Firmware | v1.13 | v1.13 (identical) |

The firmware detects sensors at runtime and adapts — using `brewingstation2` hardware with the same sketch works as long as `config.h` pin assignments match the PCB.
