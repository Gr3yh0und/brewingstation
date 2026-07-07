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

**First-time setup:** Copy `include/config_example.h` → `include/config.h` and fill in the MQTT broker IP and OTA password (these seed the runtime-configurable defaults — see below). WiFi credentials are *not* set in `config.h`; provision them via the WiFiManager captive portal on first boot instead.

---

## Key Architecture Decisions

**PCF8574 for LEDs:** All 6 power-level LEDs are driven via PCF8574 I2C expander, not direct GPIOs. This freed 6 pins and avoided a conflict with the ESP32-C6's USB-HS pins (IO12/13). PCF8574 outputs are active-LOW: write 0 to turn LED on, 1 to turn it off. The global is `ledExpander` (PCF8574 instance). Functions: `setAllLEDs()`, `ledSingleOn()`, `setLED()`, `ledBootAnimation()`, `ledEmergencyBlink()`.

**Software SPI (shared bus):** MAX31865 and BME680 share MOSI(IO4)/MISO(IO5)/CLK(IO6). CS pins are IO7 and IO2 respectively. Both use software SPI (Adafruit constructors with explicit pin args), not hardware SPI.

**Level shifter (BSS138 module):** IDS2 signals (IO18/19/20) go through the BSS138 4-channel bidirectional module. The relay (IO21) is driven directly at 3.3V — the relay module must accept 3.3V input on its IN pin.

**arduino-timer library:** `contrem/arduino-timer` (unpinned, latest). Uses the current API: `#include <arduino-timer.h>`, `Timer<>`, `every()`, `tick()`. Callbacks must have signature `bool cb(void*)` returning `true` to repeat — the four internal timer callbacks use thin `_cb_*` wrappers so the underlying `void` functions stay callable directly.

**GPIO_EXT:** `GPIO_EXT_PIN` (IO10) is defined in firmware (gpio5 MQTT subsystem) but J2 connector was removed. IO10 is physically on the DevKit; wire directly if needed. BSS138 CH1 (both LV and HV sides) is nc in schematic.

**I2C bus:** SDA=IO23, SCL=IO22. Both OLEDs (0x3C, 0x3D) and PCF8574 (0x20) share this bus. `Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN)` is called once in setup() — do not add a second call.

**USB-CDC:** The ESP32-C6 has built-in USB-CDC. `Serial` works over USB without a CH343/CP2102. `-DARDUINO_USB_CDC_ON_BOOT=1` and `-DARDUINO_USB_MODE=1` in `platformio.ini` enable this.

**WiFiManager provisioning:** `tzapu/WiFiManager` replaces the old hardcoded `WiFi.begin(ssid, password)`. It uses the ESP32's own NVS-stored Wi-Fi credentials when present; otherwise (or on connect failure, or if any panel button is held for ~2s at power-on) it opens a captive-portal AP so the device can be provisioned over its own WiFi network without a reflash. The portal's custom fields also cover the MQTT broker address, MQTT topic prefix (`cfgMqttRoot`/`cfgMqttDevice`), and OTA password — these are seeded from `config.h` on first boot and then persisted to `/netconfig.json` (LittleFS) whenever the portal form is submitted, via `loadNetConfig()`/`saveNetConfig()` in `src/main.cpp`. Because the topic prefix is now runtime-configurable, all 13 subscribe-side MQTT topics (`TOPIC_*`) are runtime `char` buffers built once in `setup()` by `computeSubscribeTopics()`, not compile-time macros — keep this in mind if adding a new subscribed topic.

**Two-tier LittleFS persistence:** brew-session tuning (`powerCap`/`deviceMode`/PID state+tunings+setpoint, set via MQTT) lives in `/settings.json`; network/identity config (broker/topic-prefix/OTA password, set via the WiFiManager portal) lives in `/netconfig.json`. Kept as two separate files/functions (`loadSettings`/`saveSettings` vs. `loadNetConfig`/`saveNetConfig`) since they're written from different code paths (MQTT callback vs. portal save-callback) and loaded at different points in `setup()` (`loadNetConfig()` must run before Wi-Fi connects; `loadSettings()` must run after `setup_pid()`, which would otherwise stomp `PID_Setpoint`).

**Partition table:** `default_8MB.csv` (dual ~3.19MB OTA app slots + 1.5MB LittleFS) — the board is the N8 (8MB flash) module, but the project was left on PlatformIO's stock 4MB `default.csv` until the WiFiManager work needed the extra headroom. If flash usage ever gets tight again, this is already the largest dual-OTA-slot table available; the fallback would be dropping one OTA slot (`huge_app.csv`-style) rather than shrinking LittleFS.

**Web dashboard + browser OTA:** Built-in `WebServer`/`Update` (both ship with the arduino-esp32 core — no `lib_deps` entry needed, same as `WiFi`/`ArduinoOTA`). `setup_web_server()` registers three tabbed pages sharing a nav bar (`webPageHeader(active)` in `src/main.cpp`):
  - `GET /` — read-only status page (sensors, setpoint, PID/mode, induction power/cap, relay, GPIO5, brew timer, RSSI, uptime, heap, version). Meta-refresh every 5s, deliberately no JS/AJAX. Unauthenticated.
  - `GET|POST /update` — firmware upload form + multipart handler; streams into the inactive OTA partition via `Update.begin/write/end`, `ESP.restart()`s on success. Also links to the GitHub releases page and has a "Check for update" button — that check runs client-side (browser `fetch()` against `api.github.com/repos/.../releases`, filtered to `3.*` tags, compared against `VERSION`), deliberately not on-device, to avoid needing a TLS stack/root CA in firmware just for a version string.
  - `GET /config` + `GET /config/download` — current runtime config (broker/topic prefix/WiFi/power cap/mode/PID); the page masks the OTA password, the download link serves the same fields (via shared `buildConfigJson()`) as a plaintext `.json` attachment for backup.
  `/update` and `/config*` are gated behind HTTP Basic Auth using `cfgOtaPassword` — the same password ArduinoOTA uses, so there's only one password to manage. `webServer.handleClient()` must be called every `loop()` iteration, same cadence as `ArduinoOTA.handle()`. The shared nav bar (`webPageHeader()`) shows `HOSTNAME`/`VERSION`; `GITHUB_REPO_URL` in `src/main.cpp` points the releases link/update-check at `Gr3yh0und/brewingstation` — update it if the repo is ever forked/renamed.

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

Work through roughly in this order — each stage assumes the previous one passed.

**1. Power-on & connectivity**
- [ ] Board powers up clean — check 3V3/5V rails before first boot, no shorts/smoke
- [ ] Serial console (USB-CDC) shows boot log; both OLEDs run the boot animation at 0x3C/0x3D
- [ ] PCF8574 LED expander detected (no "not found" warning); LED boot animation runs
- [ ] WiFi connects via the WiFiManager captive portal (hold any panel button ~2s at power-on to force it, or let it auto-open on first boot); mDNS resolves `ESP-BREWING.local`
- [ ] MQTT connects to the broker; `device` status topic publishes on schedule
- [ ] NTP sync succeeds — Display 1 switches from uptime to wall-clock time
- [ ] OTA update workflow end-to-end: flash over LAN, confirm reboot into new firmware. Set a real `OTA_PASSWORD` in `config.h` first — it's still the `"change-me"` placeholder. Work through this together to build a repeatable OTA workflow, not just a one-off test.

**2. Sensors**
- [ ] All 3 DS18B20s enumerate over OneWire (check `numberOfDevices` / syslog output, not ghosts)
- [ ] MAX31865 (PT100X) detected and reads a plausible room temperature (not the fault sentinel)
- [ ] BME680 detected; temperature/humidity/pressure all read plausible values
- [ ] Confirm `PID_SENSOR_INDEX` actually points at the sensor you intend to drive the PID (physically, not just by config comment)
- [ ] Two-point calibration for each sensor type: ice water (0°C) and boiling water (100°C), update `SENSOR_*_CAL_POINT{1,2}_{RAW,REF}` in `config.h` from the real readings (currently identity defaults — no correction applied)

**3. Induction cooker**
- [ ] Relay (WHITE) switches cooker mains on/off correctly
- [ ] Each of the 6 power steps (P0–P5 / 0–100%) actually changes cooker output — verify against the cooker's own display, not just firmware state
- [ ] Intermediate power percentages (PWM cycling between adjacent steps) look smooth, not jarring
- [ ] RX interrupt (BLUE) decodes cooker error codes correctly — trigger at least one real fault if safely possible (e.g. unplug the pot) and confirm the decoded code matches
- [ ] Fan cooldown delay: relay stays on ~60s after power-off, matches `INDUCTION_FAN_DELAY`

**4. Buttons & panel LEDs**
- [ ] Measure real ADC readings per button (serial monitor), adjust `BUTTON_THRESHOLD_B1–B5` in `config.h` if they don't match the defaults
- [ ] Each button sets the correct induction power and lights the matching LED
- [ ] Debounce works — no double-triggers on a single press
- [ ] Button-set power is correctly clamped by `powerCap` (regression check for the bug fixed this session)

**5. PID tuning**
- [ ] Re-tune P/I/D for the actual vessel — current defaults (`45.27` / `0.2371` / `7.2`) were carried over from earlier hardware generations, not derived from this build
- [ ] Verify PID holds setpoint without excessive oscillation or overshoot
- [ ] Exercise `pid/enable`, `pid/reset`, `pid/p`, `pid/i`, `pid/d`, `pid/setpoint` via MQTT and confirm each behaves as fixed this session (reset actually clears the integral term, enable is ignored in slave mode, malformed payloads are rejected not zeroed)

**6. Safety systems**
- [ ] Sensor staleness: disconnect the primary sensor mid-heat, confirm `safetyShutdown()` fires within `SENSOR_STALE_TIMEOUT_MS`
- [ ] Thermal runaway: confirm shutdown fires if input exceeds setpoint + `PID_SAFETY_OVERSHOOT`
- [ ] Induction fault codes (E3/E7/E8) trigger safety shutdown, buzzer alarm, and LED emergency blink
- [ ] Watchdog: confirm a deliberately stalled `loop()` (temporary test build) reboots within `WDT_TIMEOUT_S`

**7. Buzzer**
- [ ] Step-complete tone (brew timer expiry) is audible and distinct from the alarm tone
- [ ] Safety-alarm triple beep is audible over ambient brewing noise

**8. Timer & general MQTT control**
- [ ] `timer/set` + `timer/ctl` (start/pause/reset) behave correctly end-to-end, including the expiry beep/syslog fix from this session
- [ ] `relay/set` and `gpio5/set` with a `duration` auto-off correctly at expiry
- [ ] `device/mode` switching between standalone/slave correctly resets PID state both directions

**9. Case fit**
- [ ] All connectors align with the printed case's panel cutouts (see CASE.md); nothing binds when the lid closes

**10. Soak test**
- [ ] Run for several hours under normal use; watch `device.heap` for a downward trend (leak) and confirm WiFi/MQTT reconnect resilience after a deliberate broker/router restart

### Firmware only
- [x] Sensor calibration: two-point linear calibration (`corrected = raw * slope + offset`) computed at startup from config-defined (raw, reference) point pairs. `computeCalibration()` + `setup_sensor_calibration()` in `src/main.cpp`; per-sensor points (DS18B20 array, MAX31865, BME680) in `config.h` / `config_example.h` as `SENSOR_{DS,PT100X,BME680}_CAL_POINT{1,2}_{RAW,REF}`. Not yet persisted to flash or settable via MQTT — LittleFS persistence infra exists now (see item below) but calibration points aren't wired into it; still config.h-only.
- [x] WiFi setup via captive portal (WiFiManager) — `wm.autoConnect()`/`startConfigPortal()` in `src/main.cpp`, opens on first boot/failed connect or by holding any panel button ~2s at power-on. Credentials live in the ESP32's own NVS (not `config.h`). The same portal's custom fields also provision the MQTT broker/topic-prefix/OTA password (see items below).
- [x] OTA via browser upload (web portal) — built-in `WebServer`/`Update`, `GET|POST /update` in `setup_web_server()`, gated behind HTTP Basic Auth with `cfgOtaPassword`. See "Web dashboard + browser OTA" above.
- [x] Web interface: live dashboard — `GET /` in `setup_web_server()`, read-only, meta-refresh every 5s (no JS/AJAX by design — kept deliberately simple). Tabbed nav also links to `/update` and a `/config` page (current MQTT/WiFi/PID config, OTA password masked, with a JSON download for backup). Same section as above.
- [x] Config persistence via LittleFS (survive reboot) — power cap, device mode, PID
      state/tunings/setpoint saved to `/settings.json`, reloaded at boot; MQTT
      broker/topic-prefix/OTA password saved to `/netconfig.json` via the WiFiManager portal
- [x] Configurable MQTT topic prefix at runtime — `cfgMqttRoot`/`cfgMqttDevice` in
      `src/main.cpp`, set via the WiFiManager portal, persisted to `/netconfig.json`. All 13
      subscribe-side topics converted from compile-time macros to runtime buffers
      (`computeSubscribeTopics()`) to support this.
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
