# Brewing Station 3 — Claude Code Context

This is a PlatformIO project for an automated beer brewing temperature controller.

---

## Project State (as of 2026-05-25)

Active development. Firmware compiles and is feature-complete for the core brewing loop. Hardware not yet assembled on the new ESP32-C6 board — the KiCad schematic and firmware are being developed in parallel.

KiCad schematic is script-generated (`wire_schematic.py`) and currently passes ERC with **0 violations** (4 ignored tests).

---

## Hardware

**MCU:** ESP32-C6-WROOM-1U on ESP32-C6-DevKitC-1-N8  
RISC-V single-core 160 MHz, WiFi 6, BLE 5.3, USB-CDC built-in (no external USB-UART needed), 3.3V I/O.

**Key peripherals:**
- GGM IDS2 induction cooker — 3-wire proprietary serial protocol via BSS138 level shifter
- DS18B20 × 3 — OneWire on IO11
- MAX31865 (PT1000/PT100) — software SPI, CS on IO7
- BME680 — software SPI, CS on IO2; shares MOSI/MISO/CLK (IO4/IO5/IO6) with MAX31865
- 2× SSD1306 OLED 128×64 — I2C 0x3C / 0x3D on IO23(SDA)/IO22(SCL)
- PCF8574 I2C GPIO expander (0x20) — drives 6 power-level LEDs on P0–P5 (active-LOW)
- BSS138 4-channel bidirectional level shifter module (3.3V↔5V) — IDS2 lines (IO18/19/20), GPIO_EXT (IO10)
- Relay module — IO21 (direct 3.3V, no level shifter; use 3V3-compatible relay module; connects via 3-pin Dupont header K1)
- 6-button resistor ladder — IO1 (ADC1)
- **Safe GPIO rule:** IO8 and IO9 are strapping pins (DevKit BOOT button) — do not assign. IO12/13 are USB D-/D+ — do not assign. IO14 is not exposed on DevKitC-1 — do not assign.

---

## Project Structure

```
brewingstation3/
├── platformio.ini          ← build config, all lib deps declared here; also defines env:native for tests
├── .gitignore              ← excludes .pio/, include/config.h
├── src/
│   └── main.cpp            ← all firmware (~1150 lines)
├── include/
│   ├── config_example.h    ← all pin defs + tunable defaults (committed)
│   ├── config.h            ← your secrets: WiFi, MQTT, OTA password (gitignored)
│   └── pure_logic.h        ← hardware-independent logic (calibration math, button bucket mapping,
│                              induction error strings) — pulled out of main.cpp so it's unit-testable
├── test/
│   └── test_pure_logic/    ← Unity tests for pure_logic.h, run via `pio test -e native`
└── kicad/
    ├── brewingstation3.kicad_sch   ← KiCad 10 schematic (generated — do not hand-edit)
    ├── brewingstation3.kicad_pro   ← KiCad 10 project file
    ├── wire_schematic.py           ← canonical generator: components + full wiring (run this)
    └── place_components.py         ← old generator: components only, no wiring (reference)
```

**Tests:** `pio test -e native` runs `test/test_pure_logic` on the host (no ESP32 toolchain, no device). Covers `computeCalibration()`, `getButtonBucket()`, and `inductionErrorString()` from `include/pure_logic.h`. `env:native` excludes `src/` from the build (`build_src_filter = -<*>`) since `main.cpp` pulls in Arduino/ESP32-only headers that don't exist on native. Wired into CI as a separate `test` job in `.github/workflows/build-brewingstation3.yml`. When adding new pure logic (no `digitalWrite`/`millis`/etc.), prefer putting it in `pure_logic.h` and testing it there rather than burying it in `main.cpp`.

**To build:** Open this folder in VS Code with PlatformIO IDE installed. Build/Upload/Monitor via the status bar icons, or:
```sh
pio run -e esp32c6
pio run -e esp32c6 -t upload
pio device monitor
```

**First-time setup:** Copy `include/config_example.h` → `include/config.h` and fill in WiFi credentials, MQTT broker IP, OTA password.

---

## Key Architecture Decisions

**PCF8574 for LEDs:** All 6 power-level LEDs are driven via PCF8574 I2C expander, not direct GPIOs. This freed 6 pins and avoided a conflict with the ESP32-C6's USB-HS pins (IO12/13). PCF8574 outputs are active-LOW: write 0 to turn LED on, 1 to turn it off. The global is `ledExpander` (PCF8574 instance). Functions: `setAllLEDs()`, `ledSingleOn()`, `setLED()`, `ledBootAnimation()`, `ledEmergencyBlink()`.

**Software SPI (shared bus):** MAX31865 and BME680 share MOSI(IO4)/MISO(IO5)/CLK(IO6). CS pins are IO7 and IO2 respectively. Both use software SPI (Adafruit constructors with explicit pin args), not hardware SPI.

**Level shifter (BSS138 module):** IDS2 signals (IO18/19/20) go through the BSS138 4-channel bidirectional module. The relay (IO21) is driven directly at 3.3V — the relay module must accept 3.3V input on its IN pin.

**arduino-timer library:** `contrem/arduino-timer` (unpinned, latest). Uses the current API: `#include <arduino-timer.h>`, `Timer<>`, `every()`, `tick()`. Callbacks must have signature `bool cb(void*)` returning `true` to repeat — the four internal timer callbacks use thin `_cb_*` wrappers so the underlying `void` functions stay callable directly.

**GPIO_EXT:** `GPIO_EXT_PIN` (IO10) is defined in firmware (gpio5 MQTT subsystem) but J2 connector was removed. IO10 is physically on the DevKit; wire directly if needed. BSS138 CH1 (both LV and HV sides) is nc in schematic.

**I2C bus:** SDA=IO23, SCL=IO22. Both OLEDs (0x3C, 0x3D) and PCF8574 (0x20) share this bus. `Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN)` is called once in setup() — do not add a second call.

**USB-CDC:** The ESP32-C6 has built-in USB-CDC. `Serial` works over USB without a CH343/CP2102. `-DARDUINO_USB_CDC_ON_BOOT=1` and `-DARDUINO_USB_MODE=1` in `platformio.ini` enable this.

---

## Config Reference (key defines in include/config_example.h)

```cpp
// OneWire
SENSOR_BUS_PIN          IO0

// SPI bus (shared)
SENSOR_PT100X_CS_PIN    IO4   // chip select (closest left-side pin to module, avoids trace crossing)
SENSOR_PT100X_DI_PIN    IO7   // MOSI
SENSOR_PT100X_DO_PIN    IO6   // MISO
SENSOR_PT100X_CLK_PIN   IO5   // clock
SENSOR_BME680_CS_PIN    IO2

// I2C
I2C_SDA_PIN             IO23
I2C_SCL_PIN             IO22

// IDS2 induction cooker (via BSS138 level shifter)
INDUCTION_PIN_WHITE     IO18  // relay enable
INDUCTION_PIN_YELLOW    IO19  // TX
INDUCTION_PIN_BLUE      IO20  // RX interrupt

// Other outputs
RELAY_PIN               IO21  // direct 3.3V — 3-pin Dupont header K1 to relay module
GPIO_EXT_PIN            IO10  // via BSS138 level shifter

// Button ladder (ADC1 only)
BUTTON_ADC_PIN          IO1

// PCF8574 LED expander
PCF8574_ADDR            0x20
PCF8574_PIN_LED_0       P0
PCF8574_PIN_LED_20      P1
PCF8574_PIN_LED_40      P2
PCF8574_PIN_LED_60      P3
PCF8574_PIN_LED_80      P4
PCF8574_PIN_LED_100     P5
```

---

## Backend

- **Raspberry Pi** at fixed LAN IP (set in `config.h` as `SERVER_ADDRESS`)
- **Mosquitto** MQTT broker on port 1883
- **Syslog** listener on UDP port 514
- **InfluxDB + Telegraf + Grafana** for storage and visualization
- No cloud services. LAN-only.
- MQTT root: `cave/brewery/...` (configurable via `MQTT_ROOT_PATH` / `MQTT_DEVICE`)

---

## Pending Work (long-term backlog)

### Hardware / PCB impact (decide before PCB is finalised)
- [x] **Buzzer firmware** — `setup_buzzer()` + `buzzerBeep()`/`buzzerAlarm()` via LEDC (`ledcAttach`/`ledcWriteTone`) in `src/main.cpp`. `buzzerBeep()` on brew timer expiry, `buzzerAlarm()` (triple beep) on `safetyShutdown()`. Tunable via `BUZZER_FREQ_HZ` / `BUZZER_ALARM_FREQ_HZ` / `BUZZER_BEEP_MS` in `config.h`.
- [x] **Multiple MAX31865 sensors** — deliberately dropped; single MAX31865 only

### Post-assembly validation (needs the physical board — not yet soldered)
- [ ] OTA update workflow: verify `ArduinoOTA` end-to-end on real hardware once assembled — flash over LAN, confirm reboot into new firmware, confirm `OTA_PASSWORD` in `config.h` is a real password (not the `"change-me"` placeholder default) before relying on it. Work through this together to establish a repeatable OTA workflow, not just a one-off test.

### Firmware only
- [x] Sensor calibration: two-point linear calibration (`corrected = raw * slope + offset`) computed at startup from config-defined (raw, reference) point pairs. `computeCalibration()` + `setup_sensor_calibration()` in `src/main.cpp`; per-sensor points (DS18B20 array, MAX31865, BME680) in `config.h` / `config_example.h` as `SENSOR_{DS,PT100X,BME680}_CAL_POINT{1,2}_{RAW,REF}`. Not yet persisted to flash — depends on the LittleFS config-persistence item below.
- [ ] WiFi setup via captive portal (WiFiManager)
- [ ] OTA via browser upload (web portal) — ArduinoOTA already works over LAN
- [ ] Web interface: live dashboard
- [ ] Config persistence via LittleFS (survive reboot)
- [ ] Configurable MQTT topic prefix at runtime
- [ ] Temperature ramp (°C/min) control
- [ ] Mash step programmer
- [ ] External MQTT power consumption monitoring (Tasmota integration)
- [ ] Home Assistant MQTT auto-discovery

### Done / no longer needed
- [x] Code review bug pass (8 bugs fixed in `src/main.cpp`): OOB write in the induction RX ISR (`inputBuffer` off-by-one), sensor-staleness tracking ignoring non-DS18B20 PID sensors, `pid/reset` not actually resetting PID_v1's integral term, malformed-MQTT-payload validation (`induction/cap`, `pid/setpoint`, `pid/p`/`i`/`d`), panel buttons bypassing `powerCap`, `millis()` wraparound in all deadline comparisons (new `deadlineReached()` helper in `pure_logic.h`, tested), ISR calling a non-`IRAM_ATTR` function, and a pointless reassignment racing the ISR in `setup_induction()`.
- [x] Code review judgment-call pass: regenerated `include/config.h` from `config_example.h` (it described an old "custom carrier PCB" revision — wrong pins throughout, including `RELAY_PIN` on a boot-strapping pin and `GPIO_EXT_PIN` on a pin not exposed on the DevKitC-1); replaced fragile PT100X float-equality sensor detection with a threshold; moved brew-timer expiry out of `display_update()` into a dedicated `checkTimerExpiry()` called every `loop()` iteration; narrowed the `pid/#` MQTT subscription to `pid/+` so the device no longer round-trips its own status publishes; gated `pid/enable` on standalone mode so it can't set a latent `PID_state` while in slave mode.
- [x] BME280 → BME680 sensor switch (firmware + schematic + BOM + config)
- [x] Screw terminal connector list in README
- [x] Free GPIO expansion headers (J10 power tap, J11 UART, J12 GPIO — IO15/IO22/IO23)
- [x] Button resistor ladder moved fully to PCB (R9 pull-up + R10–R14 ladder); J5 redesigned to 7-pin (bare switches on panel)
- [x] PCF8574 P6/P7 wired to R15/R16/LED_P6/LED_P7 spare LED outputs on J6 (now 9-pin)
- [x] J2 (GPIO_EXT connector) removed; IO10 free; BSS138 CH1 nc

---

## Schematic

KiCad 10 schematic in `kicad/`. **`wire_schematic.py` is the canonical generator** — run `python wire_schematic.py` to regenerate `brewingstation3.kicad_sch` after any changes. `place_components.py` is an older partial script (components only, no wiring) kept for reference. The old Fritzing file (from the parent repo) is obsolete.

### KiCad schematic generation conventions (learned from ERC debugging)

**Coordinate system:** KiCad schematic Y-axis is downward. In `wire_schematic.py`, `cx(x) = snap(x)` and `cy(y, n) = snap_y(y, n)` snap to the 2.54mm grid. Pin endpoints use `lpin(cx, cy, idx, n, bw)` / `rpin(...)`.

**Power symbol pin type:** All GND/+3V3/+5V supply symbols use `power_in` type pins (not `power_out`). `power_out` on multiple symbols of the same net triggers ERC "Power output and Power output connected." KiCad's own GND/VCC library symbols use `power_in`.

**PWR_FLAG:** Add one PWR_FLAG (`power_out`) per power net that has no other `power_out` driver. GND and +5V get explicit flags (placed co-located with an existing power symbol). +3V3 does NOT get a flag — U1 (ESP32-C6) already drives that net with a `power_out` pin from its onboard LDO. Adding a flag on +3V3 causes "Power output and Power output connected."

**Adjacent power symbols — staggered stubs:** When GND and +3V3 are on adjacent pins (2.54mm apart), use different stub lengths so the symbols are diagonally offset and don't overlap:
- `pwr_l("GND",  px, py, stub=P)`      → symbol 2.54mm left of pin
- `pwr_l("+3V3", px, py, stub=2*P)`    → symbol 5.08mm left of pin
- Three consecutive power pins: use `stub=P`, `stub=2*P`, `stub=3*P`

**Shared SPI bus — SDO pin type:** When two sensors share MISO (SDO), both pins must be `passive` type, not `output`. Two `output` pins on the same net trigger ERC "Pins of type Output and Output are connected." Use `PASS` in the pin-list tuples for SDO/DRDY on MAX31865 and BME680.

**Net labels (angle 180):** Labels with `angle=180` have their connection point on the RIGHT and text extending left — place them at the end of a wire stub going left from the component pin.

---

## Rules for Claude

- Default to editing only files in this `brewingstation3/` folder. `brewingstation/` (V1 firmware, both breadboard and PCB hardware variants) is a separate, still-functional project — bug fixes and doc updates there are fine when asked, but don't restructure its architecture or backport v3 patterns into it without being asked; it's frozen in design, not in maintenance.
- The canonical firmware source is `src/main.cpp`. Do not recreate or edit any `.ino` file.
- All library changes go in `platformio.ini`, not installed manually.
- Keep `include/config_example.h` free of secrets — it is committed. Secrets live in `include/config.h` (gitignored).
