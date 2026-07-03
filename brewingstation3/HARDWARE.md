# Brewing Station 3 — Hardware Reference

**MCU:** ESP32-C6-WROOM-1U on ESP32-C6-DevKitC-1-N8  
RISC-V single-core 160 MHz, WiFi 6, BLE 5.3, 3.3V I/O, USB-CDC built-in.

---

## Sensors

| Sensor | Interface | Purpose |
|---|---|---|
| DS18B20 (×3) | OneWire (IO11) | Mash temperature — sensor 0 is the PID input |
| MAX31865 / PT100X | Software SPI (CS: IO4) | High-accuracy RTD temperature |
| BME680 | Software SPI (CS: IO2) | Ambient temperature / humidity / pressure |

MAX31865 and BME680 share the same MOSI/MISO/CLK bus (IO7/IO6/IO5) with separate CS pins.

---

## Actuators & UI

| Component | Interface | Purpose |
|---|---|---|
| GGM IDS2 induction cooker | 3-wire serial via BSS138 level shifter (IO18/IO19/IO20) | Heat source |
| Relay module (K1) | IO21 (direct 3.3V, no level shifter) | General-purpose relay (pump / Rührwerk) — external module via 3-pin Dupont header |
| Magnetic buzzer SUMMER CPM 121 (BZ1) | IO0 → BC547B NPN → buzzer → +3V3 | Audible alerts (step complete, safety events) |
| PCF8574 I2C GPIO expander | I2C 0x20 (IO23/IO22) | Drives 8 power-level LEDs (active-LOW) |
| 6× LEDs (LD1–LD6) via P0–P5 | PCF8574 + R2–R7 (330Ω) → J15–J20 (pin 3) | IDS2 power state indicator — lit LED = current power level (0/20/40/60/80/100%) |
| 2× spare LEDs (LD7–LD8) via P6–P7 | PCF8574 + R15–R16 (330Ω) → J21, J22 (pin 2) | Spare outputs for custom use |
| 6× buttons (SW2–SW7) | Resistor ladder (IO1 / ADC1) → J15–J20 (pin 2) | Sets IDS2 cooker power level directly (0/20/40/60/80/100%) |
| 2× SSD1306 OLED 128×64 | I2C 0x3C / 0x3D (IO23/IO22) | Temperature + status display |

---

## Level Shifter

BSS138 4-channel bidirectional level shifter module (VCCA=3.3V, VCCB=5V) bridges the ESP32-C6's 3.3V GPIO to the 5V signalling required by the induction cooker (WHITE/YELLOW/BLUE) on CH2–CH4. CH1 is not connected (IO10 is broken out directly via J13). The relay module is driven directly at 3.3V — select a relay module with 3.3V-compatible opto-coupler input.

---

## Power Chain

IDS2 VCC tap or PExt → PJump (source select) → PSw (on/off) → +5V rail → DevKit VIN + relay module + BSS138 VCCB

---

## PCB Connectors

**Female sockets (2.54mm) — module plugs in:**

| Ref | Component | Socket |
|---|---|---|
| U1 | ESP32-C6-DevKitC-1 | 2× 1×16 female socket strip — DevKit is swappable |
| U3 | BSS138 level shifter module | 2×6 female, 2.54mm |
| U5 | MAX31865 module | 1×7 female, 2.54mm |

**External module connector (Dupont cables):**

| Ref | Component | Connector |
|---|---|---|
| K1 | Relay module | 1×3 male header, 2.54mm (VCC / GND / IN) — relay load contacts on module's own screw terminals |

**Male headers (2.54mm) — jumper cap or dupont wires plug in:**

| Ref | Purpose | Pins |
|---|---|---|
| J4 | PSource — power source selector | 3 (IDS2 tap / PSOURCE_OUT / PExt); short 1-2 or 2-3 |
| J10 | PWR_TAP — voltage reference | 3 (GND, +3V3, +5V) |
| J11 | UART — IO16 TX / IO17 RX | 3 (GND, TX, RX) |

**Phoenix screw terminal GPIO breakouts:**

| Ref | Purpose | Pins |
|---|---|---|
| J12 | GPIO15 breakout | 2 (GND, GPIO15) |
| J13 | GPIO10 breakout | 2 (GND, GPIO10) |
| J14 | GPIO3_ADC breakout | 2 (GND, GPIO3_ADC) |

**IDS2 connector (internal):**

| Ref | Purpose | Type |
|---|---|---|
| J1 | GGM IDS2 induction cooker | Phoenix MPT 5-pin, 2.54mm — IDS2 cable feeds through case wall, individual wires screwed in (IDS2_5V / GND / WHITE / YELLOW / BLUE) |

---

## External Wiring — Screw Terminal Connectors

All screw terminal connections use **Phoenix Contact MPT 0,5/X-2,54** PCB terminal blocks (2.54mm pitch, screw clamp, 6A / 160V, green).  
Reichelt article numbers: [**PHC 1725656**](https://www.reichelt.de/de/de/shop/produkt/leiterplattenklemme_2-polig_rm_2_54-60874) (2-pin), [**PHC 1725669**](https://www.reichelt.de/de/de/shop/produkt/leiterplattenklemme_3-polig_rm_2_54_mm-60875) (3-pin), [**PHC 1725672**](https://www.reichelt.de/de/de/shop/produkt/leiterplattenklemme_4-polig_rm_2_54_mm-60876) (4-pin), [**PHC 1725685**](https://www.reichelt.de/de/de/shop/produkt/leiterplattenklemme_5-polig_rm_2_54_mm-255909) (5-pin).

| Ref | Connector | Pins | Wires to |
|---|---|---|---|
| J3 | External 5V power input (PExt) | 2 (+5V, GND) | Power supply / USB charger |
| SW1 | Main power switch + integrated LED | 4 (SW_A, SW_B, LED+, LED−) | Panel-mounted metal switch; LED driven via R17 (470Ω) from PSOURCE_OUT — always on when power is present |
| J15–J20 | Button + LED per power level (0/20/40/60/80/100%) | 3 (GND, BTN, LED) | J15=0%, J16=20%, J17=40%, J18=60%, J19=80%, J20=100% — each carries one panel button (SW2–SW7) and its matching LED (LD1–LD6) |
| J21, J22 | Spare LEDs (LED_P6, LED_P7) | 2 (GND, LED) | PCF8574 P6/P7 spare outputs |
| J7 | DS18B20 sensor 1 | 3 (GND, +3V3, DQ) | Wire to case-mounted connector → probe |
| J8 | DS18B20 sensor 2 | 3 (GND, +3V3, DQ) | Wire to case-mounted connector → probe |
| J9 | DS18B20 sensor 3 | 3 (GND, +3V3, DQ) | Wire to case-mounted connector → probe |
| U5 | PT1000/PT100 probe (MAX31865) | 4 (FORCE+, FORCE−, RTDIN+, RTDIN−) | Internal cable → case connector → RTD probe |
| U6 | BME680 module | 6 (VCC, GND, SCK, SDI, SDO, CSB) | Module wired directly into terminal |
| DS1 | SSD1306 OLED 0x3C | 4 (GND, VCC, SCL, SDA) | OLED module wired directly into terminal |
| DS2 | SSD1306 OLED 0x3D | 4 (GND, VCC, SCL, SDA) | OLED module wired directly into terminal |
| J12 | GPIO15 breakout | 2 (GND, GPIO15) | Spare GPIO, ADC-capable |
| J13 | GPIO10 breakout | 2 (GND, GPIO10) | Spare GPIO, ADC-capable |
| J14 | GPIO3_ADC breakout | 2 (GND, GPIO3_ADC) | Spare ADC input (ADC1_CH3) |

---

## Pin Map

DevKit pad numbers from the `PCM_Espressif:ESP32-C6-DevKitC-1` footprint.  
IO8/IO9 are strapping pins (DevKit BOOT button) — do not drive as outputs.  
IO12/IO13 are USB D−/D+ — occupied by USB-CDC, not available.  
IO14 is not exposed on the DevKitC-1 headers.  
IO16/IO17 are UART0 TX/RX — free because USB-CDC replaces the hardware UART for `Serial`.

| GPIO | DevKit pad | Signal | Status | Notes |
|---|---|---|---|---|
| IO0 | 7 | Buzzer (BUZZER_PIN) | **Used** | PWM via LEDC → BC547B NPN (Q1) → BZ1 piezo |
| IO1 | 8 | Button ladder (ADC_BTN) | **Used** | ADC1_CH1; 6-button resistor ladder |
| IO2 | 12 | BME680 SPI CS | **Used** | Software SPI chip-select |
| IO3 | 13 | GPIO3_ADC (J14) | **Broken out** | ADC1_CH3; 2-pin Phoenix screw terminal (GND + GPIO3_ADC) |
| IO4 | 3 | MAX31865 SPI CS | **Used** | Software SPI chip-select |
| IO5 | 4 | SPI CLK | **Used** | Shared bus: MAX31865 + BME680 |
| IO6 | 5 | SPI MISO (DO) | **Used** | Shared bus: MAX31865 + BME680 |
| IO7 | 6 | SPI MOSI (DI) | **Used** | Shared bus: MAX31865 + BME680 |
| IO8 | 9 | — | **Restricted** | BOOT strapping pin — do not drive as output |
| IO9 | 22 | — | **Restricted** | BOOT strapping pin — do not drive as output |
| IO10 | 10 | GPIO10 (J13) | **Broken out** | ADC1_CH2; 2-pin Phoenix screw terminal (GND + GPIO10); BSS138 CH1 nc |
| IO11 | 11 | DS18B20 OneWire bus (SENSOR_BUS_PIN) | **Used** | 4.7kΩ pull-up to 3V3 on PCB |
| IO12 | 19 | — | **Restricted** | USB D− — occupied by USB-CDC |
| IO13 | 20 | — | **Restricted** | USB D+ — occupied by USB-CDC |
| IO14 | — | — | **Not exposed** | Not on DevKitC-1 headers |
| IO15 | 29 | GPIO15 (J12) | **Broken out** | 2-pin Phoenix screw terminal (GND + GPIO15) |
| IO16 | 31 | UART_HDR TX | **Free** | UART0 TX; broken out on J11; usable as GPIO when using USB-CDC |
| IO17 | 30 | UART_HDR RX | **Free** | UART0 RX; broken out on J11; usable as GPIO when using USB-CDC |
| IO18 | 23 | Induction WHITE (relay enable) | **Used** | via BSS138 level shifter |
| IO19 | 24 | Induction YELLOW (TX) | **Used** | via BSS138 level shifter |
| IO20 | 25 | Induction BLUE (RX interrupt) | **Used** | via BSS138 level shifter |
| IO21 | 26 | Relay module control (RELAY_PIN) | **Used** | direct 3.3V to K1 Dupont header |
| IO22 | 27 | I2C SCL | **Used** | Displays (0x3C, 0x3D) + PCF8574 (0x20) |
| IO23 | 28 | I2C SDA | **Used** | Displays (0x3C, 0x3D) + PCF8574 (0x20) |

**Summary: 14 used · 4 restricted · 1 not exposed · 3 broken out (IO10→J13, IO15→J12, IO3→J14) · 2 free (IO16/IO17 on J11 UART header)**

### PCF8574 I2C GPIO expander (address 0x20)

| Pin | Signal | Notes |
|---|---|---|
| P0 | LED 0% (off) | Active-LOW |
| P1 | LED 20% | Active-LOW |
| P2 | LED 40% | Active-LOW |
| P3 | LED 60% | Active-LOW |
| P4 | LED 80% | Active-LOW |
| P5 | LED 100% | Active-LOW |
| P6 | LED_P6 spare | Active-LOW; via R15 (330Ω) to J21 pin 2 |
| P7 | LED_P7 spare | Active-LOW; via R16 (330Ω) to J22 pin 2 |

---

## Induction Cooker

**Model:** GGM IDS2

### Wiring
| Wire | GPIO | Role |
|---|---|---|
| WHITE | IO18 | Relay — switches mains power to the cooker |
| YELLOW | IO19 | TX — sends power level commands |
| BLUE | IO20 | RX — receives status/feedback (interrupt) |

All three signals pass through the BSS138 4-channel level shifter (3.3V → 5V).

### Serial Protocol
The cooker uses a proprietary pulse-width serial protocol. Each command is a 33-bit frame preceded by a start pulse.

| Signal | Duration |
|---|---|
| Start pulse (HIGH) | 25 ms |
| Gap (LOW) | 10 ms |
| Bit HIGH | 5120 µs |
| Bit LOW | 1280 µs |
| Bit separator (LOW) | 1280 µs |

Protocol documented at [InnuendoPi/MQTTDevice2](https://github.com/InnuendoPi/MQTTDevice2).

### Power Levels
| Level | Command | Power |
|---|---|---|
| P0 | `101000000000000000000000001000000` | 0% (off) |
| P1 | `100100000100000000000000010100000` | 20% |
| P2 | `100100000010000000000000001100000` | 40% |
| P3 | `100100000110000000000000011100000` | 60% |
| P4 | `100100000001000000000000000010000` | 80% |
| P5 | `100100000101000000000000100100000` | 100% |

### Intermediate Power (PWM Cycling)
Percentages between discrete levels are achieved by time-multiplexing two adjacent levels within a 20-second window.

**Example — 30%** (between P1=20% and P2=40%):
- 10 s at P1 (20%) + 10 s at P2 (40%) = effective 30%

### Status & Error Codes
| Code | Error | Trigger safety shutdown |
|---|---|---|
| 0 | OK — no fault | — |
| 1, 2 | E0 — no pot detected | — |
| 3 | E1 — circuit fault | — |
| 4, 5 | E3 — cooker overheat | yes |
| 6 | E4 — temperature sensor fault | — |
| 9 | E7 — supply voltage low | yes |
| 10 | E8 — supply voltage high | yes |
| 14 | EC — control panel fault | — |

### Relay & Fan Cooldown
WHITE relay controls cooker mains power. Stays on for **60 seconds after power-off** for fan cooldown. Configurable via `INDUCTION_FAN_DELAY` in `config.h`.

---

## Displays

Both displays are 128×64 SSD1306 OLEDs on I2C (0x3C and 0x3D). They refresh every 500ms.

### Display 1 — Status (0x3C)

Font: FreeSans9pt7b. Three rows separated by horizontal lines. Right strip (26px) carries status icons.

```
┌──────────────────────────────┬────────┐
│ Run   │ 012:34               │ [WiFi] │
├───────┴──────────────────────┤        │
│ Tgt   │ 67.5                 │  [●]   │
├──────────────────────────────┤        │
│ Out   │ 82%                  │ [MQTT] │
└──────────────────────────────┴────────┘
 ←─────────── 102px ──────────→ ← 26px→
```

**Row 1 content** (priority order):
| Priority | Label | Value | Condition |
|---|---|---|---|
| 1 | `Tmr` | `MM:SS` remaining | Brew timer running or paused |
| 2 | `Clk` | `HH:MM` | NTP time synced |
| 3 | `Run` | `MMM:SS` uptime | Fallback |

**Right status strip:**
| Position | Icon | Condition |
|---|---|---|
| Top | WiFi icon (24×24) | Always shown when Wi-Fi connected |
| Middle | Filled circle (heartbeat) | Toggles every 500ms |
| Bottom | `ERR` + code | Shown when a cooker error code is active |
| Bottom | MQTT envelope icon | Flashes briefly when an MQTT command is received |

### Display 2 — Temperatures & Power (0x3D)

Font: FreeSans18pt7b. Cross-divided into four quadrants.

```
┌──────────────────┬───────────────────┐
│                  │ PID  [fan]     3  │
│      23.5        │                   │
├──────────────────┼───────────────────┤
│                  │                   │
│      22.1        │        75         │
└──────────────────┴───────────────────┘
```

| Quadrant | Content |
|---|---|
| Top-left | `temperatures[0]` — primary mash sensor / PID input |
| Bottom-left | `temperatures[1]` — secondary sensor |
| Top-right | Power level `0`–`5`; replaced by error string when fault active |
| Bottom-right | Power `%` `0`–`100` |

---

## Button + LED Panel (external — not on PCB)

Each of J15–J20 carries one panel push-button (SW2–SW7) and its matching power-level LED (LD1–LD6) on a single 3-pin connector. For power level mapping and ADC thresholds see the [Button Mapping](README.md#button-mapping) table.

**J15–J20 pinout (3-pin Phoenix each):**

| Connector | Pin 1 | Pin 2 | Pin 3 | Panel refs |
|---|---|---|---|---|
| J15 | GND | BTN1 | LED_0 | SW2 + LD1 |
| J16 | GND | BTN2 | LED_20 | SW3 + LD2 |
| J17 | GND | BTN3 | LED_40 | SW4 + LD3 |
| J18 | GND | BTN4 | LED_60 | SW5 + LD4 |
| J19 | GND | BTN5 | LED_80 | SW6 + LD5 |
| J20 | GND | ADC_BTN | LED_100 | SW7 + LD6 |

> Pin 2 (BTN) wires to the switch; when pressed, switch shorts BTN to GND via pin 1.  
> Pin 3 (LED) wires to the LED anode; cathode returns to GND independently.

**J21, J22 pinout (2-pin Phoenix each):**

| Connector | Pin 1 | Pin 2 | Purpose |
|---|---|---|---|
| J21 | GND | LED_P6 | Spare LED output (PCF8574 P6) |
| J22 | GND | LED_P7 | Spare LED output (PCF8574 P7) |

> **Calibration note:** After assembly, press each button and print the raw `analogRead(BUTTON_ADC_PIN)` value over Serial. Adjust `BUTTON_THRESHOLD_Bx` in `config.h` to match actual readings if they differ from expected values.
