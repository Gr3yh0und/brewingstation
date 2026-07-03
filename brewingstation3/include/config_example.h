// Serial
#define SERIAL_BAUDRATE 115200

// Network Configuration
#define HOSTNAME "ESP-BREWING"
#define SSID_NAME "SSID"
#define SSID_PASSWORD "PASSWORD"
#define SERVER_ADDRESS "192.168.0.100"
#define OTA_UPDATE_PORT 8266
#define OTA_PASSWORD    "change-me"  // set a strong password; anyone on the LAN can push firmware otherwise
#define MQTT_ROOT_PATH "cave"
#define MQTT_DEVICE "brewery"

// Syslog server connection info
#define SYSLOG_SERVER SERVER_ADDRESS
#define SYSLOG_PORT 514
#define SYSLOG_APP_NAME HOSTNAME

// MQTT connection settings — set BROKER_USER/PASSWORD when the broker is configured for auth
#define BROKER_ADDRESS  SERVER_ADDRESS
#define BROKER_PORT     1883
#define BROKER_USER     NULL
#define BROKER_PASSWORD NULL

// MQTT reconnect behaviour
#define MQTT_RECONNECT_INTERVAL_MS  500  // ms between reconnect attempts
#define MQTT_RECONNECT_MAX_ATTEMPTS  20  // reboot after this many failed attempts

// ── ESP32-C6 pin assignments ─────────────────────────────────────────────────
// Board: ESP32-C6-WROOM-1U on ESP32-C6-DevKitC-1-N8
// ADC1: GPIO0-6 only. USB-CDC active on GPIO12(D-)/GPIO13(D+) — do not use for other functions.
// Strapping pins — avoid driving as outputs:
//   GPIO8 (pad 9)  — boot mode (LOW at reset = download mode); DevKit BOOT button
//   GPIO9 (pad 22) — same strapping role on some C6 revisions; DevKit BOOT button

// SENSOR configuration DS18B20
#define SENSOR_BUS_PIN 11       // IO11 — OneWire data bus (4.7k pull-up to 3V3)
#define PID_SENSOR_INDEX 0      // Which DS18B20 index feeds the PID
#define SENSOR_RESOLUTION 11    // Resolution of DS18B20 sensors (9–12 bit)
#define FREQUENCY_READTEMP 100  // Sensor read interval (ms)
#define FREQUENCY_STATUS 600    // MQTT status publish interval (ms)

// SENSOR configuration PT100X (MAX31865) — software SPI
#define SENSOR_PT100X_CS_PIN  4   // IO4 — chip select
#define SENSOR_PT100X_DI_PIN  7   // IO7 — MOSI (SDI)
#define SENSOR_PT100X_DO_PIN  6   // IO6 — MISO (SDO)
#define SENSOR_PT100X_CLK_PIN 5   // IO5 — clock (SCK)
#define SENSOR_PT100X_R_REF   4300.0  // 430.0 for PT100
#define SENSOR_PT100X_R_NOM   1000.0  // 100.0 for PT100
#define SENSOR_PT100X_Config  MAX31865_2WIRE

// SENSOR configuration BME680 — software SPI (shares MOSI/MISO/CLK with MAX31865)
#define SENSOR_BME680_OFFSET   2.0
#define SEALEVELPRESSURE_HPA   (1018.00)
#define SENSOR_BME680_CS_PIN   2   // IO2 — chip select
#define SENSOR_BME680_DI_PIN   7   // IO7 — MOSI (SDI)  shared bus
#define SENSOR_BME680_DO_PIN   6   // IO6 — MISO (SDO)  shared bus
#define SENSOR_BME680_CLK_PIN  5   // IO5 — clock       shared bus

// I2C config (OLEDs + PCF8574)
#define I2C_SDA_PIN 23   // IO23
#define I2C_SCL_PIN 22   // IO22

// DISPLAY configuration for both displays
#define DISPLAY_FREQUENCY 500
#define DISPLAY_SCREEN_WIDTH 128   // OLED display width, in pixels
#define DISPLAY_SCREEN_HEIGHT 64   // OLED display height, in pixels
#define DISPLAY_OLED_RESET -1      // Reset pin # (or -1 if sharing Arduino reset pin)

// Induction cooker configuration — IDS2 signals via BSS138 4-channel level shifter (3V3 ↔ 5V)
#define FREQUENCY_INDUCTION 700    // Induction command update interval (ms)
#define INDUCTION_PIN_WHITE  18    // IO18 — relay enable (LV side); HV → IDS2 WHITE
#define INDUCTION_PIN_YELLOW 19    // IO19 — TX serial (LV side); HV → IDS2 YELLOW
#define INDUCTION_PIN_BLUE   20    // IO20 — RX interrupt (LV side); HV → IDS2 BLUE
#define RELAY_PIN 21               // IO21 — general-purpose relay (pump / Rührwerk), direct 3V3
#define GPIO_EXT_PIN 10            // IO10 — spare output (J2 connector removed; wire directly if needed)
#define BUZZER_PIN 0               // IO0 — passive piezo buzzer via MMBT2222A NPN driver; PWM via LEDC
#define BUZZER_FREQ_HZ       2000  // step-complete tone (Hz)
#define BUZZER_ALARM_FREQ_HZ 3000  // safety alarm tone (Hz)
#define BUZZER_BEEP_MS        150  // beep on-duration (ms)
#define INDUCTION_FAN_DELAY 60000  // Fan cooldown after power-off (ms); factory default 120000
#define INDUCTION_MQTT_STATUS   "induction"
#define INDUCTION_MQTT_COMMANDS "induction/set"

// Button ADC — resistor ladder on IO1 (ADC1_CH1)
#define BUTTON_ADC_PIN 1           // IO1 — ADC1 channel 1

// PCF8574 I2C GPIO expander — controls all 6 power LEDs
// I2C address determined by A0/A1/A2 strapping (all GND → 0x20)
#define PCF8574_ADDR       0x20
#define PCF8574_PIN_LED_0   0   // P0 → LED 0%   (off)
#define PCF8574_PIN_LED_20  1   // P1 → LED 20%
#define PCF8574_PIN_LED_40  2   // P2 → LED 40%
#define PCF8574_PIN_LED_60  3   // P3 → LED 60%
#define PCF8574_PIN_LED_80  4   // P4 → LED 80%
#define PCF8574_PIN_LED_100 5   // P5 → LED 100%
#define INDUCTION_LED_TEST_TIME 150

// PID default tuning — tune per vessel; all adjustable at runtime via MQTT
#define PID_DEFAULT_P 45.27
#define PID_DEFAULT_I 0.2371
#define PID_DEFAULT_D 7.2

// Safety thresholds
#define PID_SAFETY_OVERSHOOT    5.0    // °C above setpoint before thermal runaway cutoff
#define SENSOR_STALE_TIMEOUT_MS 30000  // ms without a primary sensor reading before power cut
#define PID_SETPOINT_MAX        100.0  // °C — MQTT cannot set a target above this
#define PID_MAX_P               500.0  // upper bound on P tuning accepted via MQTT
#define PID_MAX_I                10.0  // upper bound on I tuning accepted via MQTT
#define PID_MAX_D               500.0  // upper bound on D tuning accepted via MQTT

// Operating mode — 0: standalone (local PID controls power), 1: slave (MQTT-driven power only)
#define DEVICE_MODE_DEFAULT 0

// Power cap — maximum induction output % (0–100); lower to limit heat source
#define INDUCTION_POWER_CAP 100

// Watchdog timer — device reboots if loop() stalls for longer than this
#define WDT_TIMEOUT_S 30

// NTP — see https://github.com/nayarsystems/posix_tz_db for timezone strings
#define NTP_SERVER   "pool.ntp.org"
#define NTP_TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"  // Central European Time with DST

// Button ADC thresholds — adjust if your resistor ladder gives different ADC readings
// ESP32-C6 ADC is 12-bit (0-4095). Thresholds below assume 3V3 reference.
#define BUTTON_THRESHOLD_B1 3300  // above this = no press
#define BUTTON_THRESHOLD_B2 3200  // B1:  0% (off)
#define BUTTON_THRESHOLD_B3 3000  // B2: 20%
#define BUTTON_THRESHOLD_B4 2600  // B3: 40%
#define BUTTON_THRESHOLD_B5 2000  // B4: 60%
// B5: 80% (val 1–1999), B6: 100% (val == 0, shorted to GND)

// Button power levels (%) — what each button sets the induction power to
#define BUTTON_POWER_B1   0
#define BUTTON_POWER_B2  20
#define BUTTON_POWER_B3  40
#define BUTTON_POWER_B4  60
#define BUTTON_POWER_B5  80
#define BUTTON_POWER_B6 100
