#include <Arduino.h>
// Automated beer brewing station — ESP32-C6 edition
// Controls a GGM IDS2 induction cooker via PID temperature control
// Compatible with CraftBeerPi 3 and 4
// Induction cooker protocol based on: https://github.com/InnuendoPi/MQTTDevice2
// Michael Morscher — Board: ESP32-C6-WROOM-1U
#include <WiFi.h>
#include <WiFiUdp.h>
#include <time.h>
#include "esp_task_wdt.h"
#include <Wire.h>
#include <SPI.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <Update.h>
#include <Syslog.h>
#include <arduino-timer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_MAX31865.h>
#include <Adafruit_BME680.h>
#include <PID_v1.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSans24pt7b.h>
#include <PCF8574.h>
#include "config.h"
#include "pure_logic.h"

#define VERSION "3.0.0-alpha.3"

// Forward declarations
void setLED(int value);
void ledEmergencyBlink();
void relay_write_mqtt();
void gpio5_write_mqtt();
void buzzerAlarm();
void buzzerBeep();

// Feature toggles — use 0/1 so #if directives work correctly
#define SERIAL_ENABLE  0
#define MQTT_ENABLE    1
#define PUBLISH_ENABLE 1

// Operating modes
#define MODE_STANDALONE 0  // local PID computes and drives induction power
#define MODE_SLAVE      1  // power driven entirely by heater/power MQTT commands

// Sensor limits
#define SENSOR_MAXIMUM        5
#define SENSOR_MQTT_PREFIX    "sensor"    // publish root: {root}/{device}/sensor/{N}
// MAX31865 reports ~-242.02f when the RTD is open-circuit (extrapolated from a maxed-out
// fault reading). Compared with a threshold rather than exact equality: the value is
// deterministic for a given R_NOM/R_REF pair, but exact float equality is fragile across
// library versions/compilers, so anything unrealistically cold is treated as "no sensor".
#define PT100X_FAULT_THRESHOLD -240.0f

// Display
#define TEXT_ROW_HEIGHT_PX 10  // pixel height per row in the default (NULL) font

// Subscribe-side topics — runtime buffers, not compile-time macros: the root path/device
// name are now provisionable via the WiFiManager portal (cfgMqttRoot/cfgMqttDevice, up to
// 23 chars each), so they can't be pasted together by the preprocessor. Pre-computed once
// in setup() (after loadNetConfig()), same pattern as the PUBLISH_TOPIC_* buffers below.
// Sized for the worst case (23 + '/' + 23 + longest suffix "/induction/cap" = 61) + margin.
char TOPIC_INDUCTION_SET[72];
char TOPIC_INDUCTION_CAP[72];
// "+" (single-level wildcard), not "#": all pid subtopics are one level deep, and "#"
// also matches the bare "pid" topic itself — which is where this device publishes its
// own status every FREQUENCY_STATUS ms, causing a pointless publish->subscribe->parse
// round-trip of its own messages.
char TOPIC_PID_WILDCARD[72];
char TOPIC_PID_ENABLE[72];
char TOPIC_PID_RESET[72];
char TOPIC_PID_P[72];
char TOPIC_PID_I[72];
char TOPIC_PID_D[72];
char TOPIC_PID_SETPOINT[72];
char TOPIC_TIMER_SET[72];
char TOPIC_TIMER_CTL[72];
char TOPIC_DEVICE_MODE[72];
char TOPIC_RELAY_SET[72];
char TOPIC_GPIO5_SET[72];

// Publish-side topics — pre-computed in setup() to avoid per-call heap allocations.
// Same worst-case sizing rationale as the subscribe-side buffers above.
char PUBLISH_TOPIC_SENSOR_PREFIX[72];  // e.g. "cave/brewery/sensor/"
char PUBLISH_TOPIC_INDUCTION[72];      // e.g. "cave/brewery/induction"
char PUBLISH_TOPIC_PID[72];            // e.g. "cave/brewery/pid"
char PUBLISH_TOPIC_TIMER[72];          // e.g. "cave/brewery/timer"
char PUBLISH_TOPIC_DEVICE[72];         // e.g. "cave/brewery/device"
char PUBLISH_TOPIC_RELAY[72];          // e.g. "cave/brewery/relay"
char PUBLISH_TOPIC_GPIO5[72];          // e.g. "cave/brewery/gpio5"

// PCF8574 I2C GPIO expander — drives all 6 power LEDs
PCF8574 ledExpander(PCF8574_ADDR);

// Sensors & Displays
OneWire oneWire(SENSOR_BUS_PIN);
DallasTemperature sensorlist(&oneWire);
Adafruit_MAX31865 pt100x = Adafruit_MAX31865(SENSOR_PT100X_CS_PIN, SENSOR_PT100X_DI_PIN, SENSOR_PT100X_DO_PIN, SENSOR_PT100X_CLK_PIN);
bool pt100x_found = false;
Adafruit_BME680 bme680(SENSOR_BME680_CS_PIN, SENSOR_BME680_DI_PIN, SENSOR_BME680_DO_PIN, SENSOR_BME680_CLK_PIN);
bool bme680_found = false;
Adafruit_SSD1306 display(DISPLAY_SCREEN_WIDTH, DISPLAY_SCREEN_HEIGHT, &Wire, DISPLAY_OLED_RESET);
Adafruit_SSD1306 display2(DISPLAY_SCREEN_WIDTH, DISPLAY_SCREEN_HEIGHT, &Wire, DISPLAY_OLED_RESET);

// WiFi icon bitmap (24x24)
const uint8_t bitmap26[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0x80, 0x07, 0xff, 0xe0, 0x1f, 0x81, 0xf8, 0x7c, 0x00, 0x3e, 0xf0, 0x00, 0x0f, 0xe0, 0xff, 0x07, 0x03, 0xff, 0xc0, 0x0f, 0xe7, 0xf0, 0x0f, 0x00, 0xf0, 0x08, 0x00, 0x10, 0x00, 0x7e, 0x00, 0x00, 0xff, 0x00, 0x00, 0xe7, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
// MQTT message received icon bitmap (24x24)
const uint8_t bitmap27[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x03, 0x18, 0xc0, 0x03, 0x99, 0x80, 0x01, 0xdb, 0x80, 0x00, 0xff, 0x00, 0x00, 0x7e, 0x00, 0x30, 0x3c, 0x0c, 0x30, 0x18, 0x0c, 0x30, 0x00, 0x0c, 0x30, 0x00, 0x0c, 0x30, 0x00, 0x0c, 0x38, 0x00, 0x1c, 0x3f, 0xff, 0xfc, 0x1f, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
bool display_toggle = true;

// Button resistor ladder — single analog pin, 6 buttons mapped by ADC threshold
#define BUTTON_PIN         BUTTON_ADC_PIN
#define BUTTON_DEBOUNCE_MS 250
int lastButtonBucket = 0;
unsigned long lastButtonChange = 0;

// Induction cooker serial protocol timing constants — SIGNAL_HIGH/LOW(_TOL) live
// in pure_logic.h (shared with the testable pulse classifier); these two are only
// used for shaping the outgoing start pulse and aren't part of that logic.
const int SIGNAL_START    = 25;
const int SIGNAL_WAIT     = 10;

// PID controller
#define PID_MQTT_TOPIC "pid"
double PID_Setpoint;
double PID_Input;
double Output;
const double Output_Limit_Max = 100;
const double Output_Limit_Min = 0;
double PID_P = PID_DEFAULT_P;
double PID_I = PID_DEFAULT_I;
double PID_D = PID_DEFAULT_D;
bool PID_state        = false;
bool pidTuningsDirty  = false;  // set when P/I/D change; cleared after SetTunings() is called
PID myPID(&PID_Input, &Output, &PID_Setpoint, PID_P, PID_I, PID_D, P_ON_M, DIRECT);

// Network
const char *hostname = HOSTNAME;
WiFiManager wm;
WiFiClient wifiClient;
WiFiUDP udpClient;
PubSubClient mqttClient(wifiClient);
WebServer webServer(80);
bool webUpdateInProgress = false;
bool webUpdateFailed     = false;

// Runtime-configurable network/identity settings — provisioned via the WiFiManager portal's
// custom parameters (see setup()) instead of being fixed at compile time. Seeded from
// config.h so first boot (no /netconfig.json yet) behaves exactly as before.
char cfgMqttBroker[40]  = BROKER_ADDRESS;
char cfgMqttRoot[24]    = MQTT_ROOT_PATH;
char cfgMqttDevice[24]  = MQTT_DEVICE;
char cfgOtaPassword[32] = OTA_PASSWORD;
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, hostname, SYSLOG_APP_NAME, LOG_KERN);
bool message_received = false;

// Fills in the runtime TOPIC_* buffers (declared above, near the induction cooker's
// legacy compile-time defines) from cfgMqttRoot/cfgMqttDevice. Must run after
// loadNetConfig() and before subscribe_topics()/mqttCallback() are ever reachable.
void computeSubscribeTopics() {
  snprintf(TOPIC_INDUCTION_SET, sizeof(TOPIC_INDUCTION_SET), "%s/%s/induction/set", cfgMqttRoot, cfgMqttDevice);
  snprintf(TOPIC_INDUCTION_CAP, sizeof(TOPIC_INDUCTION_CAP), "%s/%s/induction/cap", cfgMqttRoot, cfgMqttDevice);
  snprintf(TOPIC_PID_WILDCARD,  sizeof(TOPIC_PID_WILDCARD),  "%s/%s/pid/+",         cfgMqttRoot, cfgMqttDevice);
  snprintf(TOPIC_PID_ENABLE,    sizeof(TOPIC_PID_ENABLE),    "%s/%s/pid/enable",    cfgMqttRoot, cfgMqttDevice);
  snprintf(TOPIC_PID_RESET,     sizeof(TOPIC_PID_RESET),     "%s/%s/pid/reset",     cfgMqttRoot, cfgMqttDevice);
  snprintf(TOPIC_PID_P,         sizeof(TOPIC_PID_P),         "%s/%s/pid/p",         cfgMqttRoot, cfgMqttDevice);
  snprintf(TOPIC_PID_I,         sizeof(TOPIC_PID_I),         "%s/%s/pid/i",         cfgMqttRoot, cfgMqttDevice);
  snprintf(TOPIC_PID_D,         sizeof(TOPIC_PID_D),         "%s/%s/pid/d",         cfgMqttRoot, cfgMqttDevice);
  snprintf(TOPIC_PID_SETPOINT,  sizeof(TOPIC_PID_SETPOINT),  "%s/%s/pid/setpoint",  cfgMqttRoot, cfgMqttDevice);
  snprintf(TOPIC_TIMER_SET,     sizeof(TOPIC_TIMER_SET),     "%s/%s/timer/set",     cfgMqttRoot, cfgMqttDevice);
  snprintf(TOPIC_TIMER_CTL,     sizeof(TOPIC_TIMER_CTL),     "%s/%s/timer/ctl",     cfgMqttRoot, cfgMqttDevice);
  snprintf(TOPIC_DEVICE_MODE,   sizeof(TOPIC_DEVICE_MODE),   "%s/%s/device/mode",   cfgMqttRoot, cfgMqttDevice);
  snprintf(TOPIC_RELAY_SET,     sizeof(TOPIC_RELAY_SET),     "%s/%s/relay/set",     cfgMqttRoot, cfgMqttDevice);
  snprintf(TOPIC_GPIO5_SET,     sizeof(TOPIC_GPIO5_SET),     "%s/%s/gpio5/set",     cfgMqttRoot, cfgMqttDevice);
}

// Timers
Timer<> timerTempStatus;
Timer<> timerTempRead;
Timer<> timerInductionStatus;
Timer<> timerDisplayUpdate;

// Operating mode
int deviceMode = DEVICE_MODE_DEFAULT;

// Power cap — runtime-adjustable ceiling applied to all induction output paths
int powerCap = INDUCTION_POWER_CAP;

// General-purpose relay
bool relayState       = false;
unsigned long relayTimerEndMs = 0;  // 0 = no active timer

// Extra GPIO (GPIO5 connector)
bool gpio5State           = false;
unsigned long gpio5TimerEndMs = 0;

// Brew timer — state machine lives in pure_logic.h (BrewTimerState) so it's
// unit-testable, including wraparound, with an injected `now`.
BrewTimerState brewTimer;

unsigned long timerRemainingMs() {
  return brewTimerRemainingMs(brewTimer, millis());
}

// Detects brew-timer expiry independent of the display refresh cadence — called every
// loop() iteration so expiry can't be delayed by DISPLAY_FREQUENCY, and status publishes
// (timer_write_mqtt) never observe a stale "running:1, remaining:0" window.
void checkTimerExpiry() {
  if (brewTimerCheckExpiry(brewTimer, millis())) {
    buzzerBeep();
    syslog.log(LOG_INFO, "Brew timer expired");
  }
}

// Sensor state
int numberOfDevices;
DeviceAddress tempDeviceAddress;
DeviceAddress sensorAddresses[SENSOR_MAXIMUM];
float temperatures[SENSOR_MAXIMUM] = { NAN, NAN, NAN, NAN, NAN };
unsigned long lastSensorReadTime = 0;

// Sensor calibration — slope/offset computed once at startup from two-point config (see config_example.h)
float dsCalSlope[SENSOR_MAXIMUM];
float dsCalOffset[SENSOR_MAXIMUM];
float pt100xCalSlope, pt100xCalOffset;
float bme680CalSlope, bme680CalOffset;

// ─── Induction cooker class ───────────────────────────────────────────────────

class induction {
  static const uint8_t PWR_STEPS[6];

  unsigned long timeTurnedoff;
  unsigned long lastInterrupt;
  bool inputStarted = false;
  unsigned char inputCurrent = 0;
  unsigned char inputBuffer[33];
  volatile uint8_t newError = 0;
  long powerSampletime = 20000;
  unsigned long powerLast;
  long powerHigh = powerSampletime;
  long powerLow = 0;

  int CMD[6][33] = {
    { 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0 },  // Off
    { 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0 },  // P1
    { 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0 },  // P2
    { 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0 },  // P3
    { 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0 },  // P4
    { 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0 }   // P5
  };

public:
  unsigned char PIN_WHITE     = INDUCTION_PIN_WHITE;
  unsigned char PIN_YELLOW    = INDUCTION_PIN_YELLOW;
  unsigned char PIN_INTERRUPT = INDUCTION_PIN_BLUE;
  int power    = 0;
  int newPower = 0;
  unsigned char CMD_CUR  = 0;
  boolean isRelayon      = false;
  boolean isInduon       = false;
  long delayAfteroff     = INDUCTION_FAN_DELAY;

  induction() {
    for (int i = 0; i < 33; i++)
      for (int j = 0; j < 6; j++)
        CMD[j][i] = (CMD[j][i] == 1) ? SIGNAL_HIGH : SIGNAL_LOW;
  }

  void millis2wait(const int &value) { delay(value); }

  void Update() {
    updatePower();
    isRelayon = updateRelay();
    if (isInduon && power > 0) {
      if (deadlineReached(millis(), powerLast + powerSampletime)) powerLast = millis();
      if (deadlineReached(millis(), powerLast + powerHigh)) {
        sendCommand(CMD[CMD_CUR - 1]);
      } else {
        sendCommand(CMD[CMD_CUR]);
      }
    } else if (isRelayon) {
      sendCommand(CMD[0]);
    }
  }

  bool updateRelay() {
    if (isInduon && !isRelayon) { digitalWrite(PIN_WHITE, HIGH); return true; }
    if (!isInduon && isRelayon) {
      if (deadlineReached(millis(), timeTurnedoff + delayAfteroff)) { digitalWrite(PIN_WHITE, LOW); return false; }
    }
    return isRelayon;
  }

  void updatePower() {
    if (power == newPower) return;
    newPower = min(100, max(0, newPower));
    power = newPower;
    timeTurnedoff = 0;
    isInduon = true;

    if (power == 0) {
      CMD_CUR = 0;
      timeTurnedoff = millis();
      isInduon = false;
      powerLow = powerHigh = 0;
      return;
    }
    PowerLevel lvl = computePowerLevel(power, powerSampletime, PWR_STEPS);
    CMD_CUR   = lvl.cmdIndex;
    powerHigh = lvl.powerHigh;
    powerLow  = lvl.powerLow;
  }

  void sendCommand(const int command[33]) {
    digitalWrite(PIN_YELLOW, HIGH); millis2wait(SIGNAL_START);
    digitalWrite(PIN_YELLOW, LOW);  millis2wait(SIGNAL_WAIT);
    for (int i = 0; i < 33; i++) {
      digitalWrite(PIN_YELLOW, HIGH); delayMicroseconds(command[i]);
      digitalWrite(PIN_YELLOW, LOW);  delayMicroseconds(SIGNAL_LOW);
    }
  }

  // IRAM_ATTR: called from readInputWrap(), an ISR — must stay resident in IRAM so it's
  // safe to call while flash cache is disabled (e.g. during OTA writes or NVS commits).
  void IRAM_ATTR readInput() {
    bool ishigh = digitalRead(PIN_INTERRUPT);
    unsigned long newInterrupt = micros();
    long signalTime = newInterrupt - lastInterrupt;
    PulseType pulse = classifyPulse(signalTime);
    if (pulse == PulseType::NOISE) return;
    if (ishigh) { lastInterrupt = newInterrupt; return; }
    if (!inputStarted) {
      if (pulse == PulseType::START) { inputStarted = true; inputCurrent = 0; }
    } else if (inputCurrent < 33) {
      if (pulse == PulseType::BIT_ONE)  inputBuffer[inputCurrent++] = 1;
      if (pulse == PulseType::BIT_ZERO) inputBuffer[inputCurrent++] = 0;
    } else {
      // Frame complete
      newError = decodeErrorCode(inputBuffer);
      inputCurrent = 0;
      inputStarted = false;
    }
  }

  uint8_t getError() const { return newError; }
  void clearError()         { newError = 0; }
};

const uint8_t induction::PWR_STEPS[6] = { 0, 20, 40, 60, 80, 100 };

induction inductionCooker;
uint8_t inductionLastError = 0;

void ARDUINO_ISR_ATTR readInputWrap() {
  inductionCooker.readInput();
}

// ─── Safety ───────────────────────────────────────────────────────────────────

bool isSensorHealthy() {
  return sensorIsHealthy(millis(), lastSensorReadTime, SENSOR_STALE_TIMEOUT_MS, numberOfDevices);
}

// Cut induction power and disable PID. Called on any safety event.
void safetyShutdown(const char *reason) {
  inductionCooker.newPower = 0;
  inductionCooker.Update();
  setLED(0);
  PID_state = false;
  ledEmergencyBlink();
  buzzerAlarm();
  char msg[80];
  snprintf(msg, sizeof(msg), "SAFETY SHUTDOWN: %s", reason);
  syslog.log(LOG_CRIT, msg);
#if SERIAL_ENABLE
  Serial.println(msg);
#endif
}

// ─── Induction timer callback ─────────────────────────────────────────────────

void handleInduction() {
  inductionCooker.Update();
  setLED(inductionCooker.power);
}

// ─── Display ──────────────────────────────────────────────────────────────────

void setup_display(Adafruit_SSD1306 &disp, unsigned int address) {
  if (!disp.begin(SSD1306_SWITCHCAPVCC, address)) {
#if SERIAL_ENABLE
    Serial.println(F("Display allocation failed"));
#endif
  }
  disp.clearDisplay();
  disp.setTextColor(WHITE);
  disp.setFont(&FreeSans24pt7b);
  disp.setCursor(29, 46);
  disp.println("OK");
  disp.display();
}

void display_update() {
  char buf[16];

  // Row 1: brew timer > NTP clock > uptime
  char row1_label[6];
  char row1_value[9];
  {
    time_t now; struct tm ti = {};
    time(&now);
    bool ntpSynced = now > 1000000000UL;
    if (ntpSynced) localtime_r(&now, &ti);
    computeRow1Display(brewTimer.running, brewTimer.paused, timerRemainingMs(),
                        ntpSynced, ti.tm_hour, ti.tm_min, millis(),
                        row1_label, row1_value);
  }

  // Display 1: Row1 / Tgt / Out
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setFont(&FreeSans9pt7b);
  display.setCursor(0, 15);
  display.drawLine(0, 20, 102, 20, 1);
  display.drawLine(0, 42, 102, 42, 1);
  display.drawLine(102, 0, 102, 63, 1);
  display.println(row1_label);
  display.println("Tgt");
  display.println(deviceMode == MODE_SLAVE ? "Pwr" : "Out");
  display.setCursor(45, 15);
  display.println(row1_value);
  display.setCursor(45, 37);
  if (PID_state && deviceMode == MODE_STANDALONE) { dtostrf(PID_Setpoint, 4, 1, buf); display.println(buf); }
  else                                              display.println("--");
  display.setCursor(45, 59);
  if (deviceMode == MODE_SLAVE) {
    snprintf(buf, sizeof(buf), "%d%%", inductionCooker.power); display.println(buf);
  } else if (PID_state) {
    snprintf(buf, sizeof(buf), "%d%%", (int)Output); display.println(buf);
  } else {
    display.println("--");
  }
  if (WiFi.status() == WL_CONNECTED)
    display.drawBitmap(104, -3, bitmap26, 24, 24, 1);
  display_toggle = !display_toggle;
  if (display_toggle) display.fillCircle(116, 31, 6, 1);
  if (inductionLastError != 0) {
    display.setFont(NULL);
    display.setTextSize(1);
    display.setCursor(104, 47);
    display.println("ERR");
    display.setCursor(107, 56);
    snprintf(buf, sizeof(buf), "%u", inductionLastError);
    display.println(buf);
  } else if (message_received) {
    display.drawBitmap(104, 42, bitmap27, 24, 24, 1);
    message_received = false;
  }
  display.display();

  // Display 2: Temperatures / Induction state
  display2.clearDisplay();
  display2.setTextColor(WHITE);
  display2.setTextSize(1);
  display2.setFont(&FreeSans18pt7b);
  display2.drawLine(0, 32, 128, 32, 1);
  display2.drawLine(70, 0, 70, 63, 1);
  display2.setCursor(0, 26);
  if (isnan(temperatures[0])) display2.println("--");
  else { dtostrf(temperatures[0], 4, 1, buf); display2.println(buf); }
  display2.setCursor(0, 61);
  if (isnan(temperatures[1])) display2.println("--");
  else { dtostrf(temperatures[1], 4, 1, buf); display2.println(buf); }
  if (inductionLastError != 0) {
    display2.setFont(NULL);
    display2.setTextSize(1);
    display2.setCursor(72, 9);
    display2.println(inductionErrorString(inductionLastError));
  } else {
    display2.setCursor(91, 27);
    snprintf(buf, sizeof(buf), "%d", inductionCooker.CMD_CUR); display2.println(buf);
  }
  if      (inductionCooker.power == 100) display2.setCursor(70, 61);
  else if (inductionCooker.power < 10)   display2.setCursor(91, 61);
  else                                   display2.setCursor(82, 61);
  snprintf(buf, sizeof(buf), "%d", inductionCooker.power); display2.println(buf);
  if (inductionCooker.isRelayon) {
    display2.drawCircle(80, 22, 8, 1);
    display2.fillTriangle(80, 20, 83, 17, 77, 17, 1);
    display2.fillTriangle(80, 24, 83, 27, 77, 27, 1);
    display2.fillTriangle(78, 22, 75, 19, 75, 25, 1);
    display2.fillTriangle(82, 22, 85, 19, 85, 25, 1);
  }
  display2.setFont(NULL);
  display2.setTextSize(1);
  display2.setCursor(73, 0);
  if (deviceMode == MODE_SLAVE)                       display2.println("SLV");
  else if (PID_state && deviceMode == MODE_STANDALONE) display2.println("PID");
  display2.display();
}

void display_writex(Adafruit_SSD1306 &disp, int row, const String &text, bool reset) {
  if (reset) disp.clearDisplay();
  disp.setTextColor(WHITE);
  disp.setTextSize(1);
  disp.setFont(NULL);
  disp.setCursor(0, row * TEXT_ROW_HEIGHT_PX);
  disp.println(text);
  disp.display();
}

// ─── LEDs (via PCF8574 I2C expander) ──────────────────────────────────────────
// PCF8574 outputs are open-drain active-LOW; we write 0 to light a LED.

static const uint8_t LED_PCF_PINS[6] = {
  PCF8574_PIN_LED_0, PCF8574_PIN_LED_20, PCF8574_PIN_LED_40,
  PCF8574_PIN_LED_60, PCF8574_PIN_LED_80, PCF8574_PIN_LED_100
};

void setAllLEDs(bool on) {
  for (int i = 0; i < 6; i++) ledExpander.write(LED_PCF_PINS[i], on ? LOW : HIGH);
}

void ledSingleOn(int idx) {
  for (int i = 0; i < 6; i++)
    ledExpander.write(LED_PCF_PINS[i], (i == idx) ? LOW : HIGH);
}

void setLED(int value) {
  value = constrain(value, 0, 100);
  if      (value == 0)  ledSingleOn(0);
  else if (value <= 20) ledSingleOn(1);
  else if (value <= 40) ledSingleOn(2);
  else if (value <= 60) ledSingleOn(3);
  else if (value <= 80) ledSingleOn(4);
  else                  ledSingleOn(5);
}

// Boot animation: knight-rider scan, then fill cascade, then final burst
void ledBootAnimation() {
  for (int c = 0; c < 2; c++) {
    for (int i = 0; i < 6; i++) { ledSingleOn(i); delay(60); }
    for (int i = 4; i >= 1; i--) { ledSingleOn(i); delay(60); }
  }
  for (int i = 0; i < 6; i++) { ledExpander.write(LED_PCF_PINS[i], LOW);  delay(50); }
  for (int i = 5; i >= 0; i--) { ledExpander.write(LED_PCF_PINS[i], HIGH); delay(50); }
  setAllLEDs(true);  delay(200);
  setAllLEDs(false); delay(100);
}

// Emergency blink: rapid all-LEDs flash — called on safety shutdown
void ledEmergencyBlink() {
  for (int i = 0; i < 5; i++) {
    setAllLEDs(true);  delay(80);
    setAllLEDs(false); delay(80);
  }
}

// ─── Buzzer (piezo, LEDC PWM tone) ────────────────────────────────────────────

void setup_buzzer() {
  ledcAttach(BUZZER_PIN, BUZZER_FREQ_HZ, 8);
}

void buzzerTone(unsigned int freq_hz, unsigned int duration_ms) {
  ledcWriteTone(BUZZER_PIN, freq_hz);
  delay(duration_ms);
  ledcWriteTone(BUZZER_PIN, 0);
}

// Single short confirmation beep — brew timer / step complete
void buzzerBeep() {
  buzzerTone(BUZZER_FREQ_HZ, BUZZER_BEEP_MS);
}

// Rapid triple beep — safety shutdown
void buzzerAlarm() {
  for (int i = 0; i < 3; i++) {
    buzzerTone(BUZZER_ALARM_FREQ_HZ, BUZZER_BEEP_MS);
    delay(BUZZER_BEEP_MS);
  }
}

// ─── Relay ────────────────────────────────────────────────────────────────────

void setRelay(bool on) {
  relayState = on;
  digitalWrite(RELAY_PIN, on ? HIGH : LOW);
}

void setGPIO5(bool on) {
  gpio5State = on;
  digitalWrite(GPIO_EXT_PIN, on ? HIGH : LOW);
}

// ─── Sensors ──────────────────────────────────────────────────────────────────

void setup_sensor_calibration() {
  const float dsRaw1[SENSOR_MAXIMUM] = SENSOR_DS_CAL_POINT1_RAW;
  const float dsRef1[SENSOR_MAXIMUM] = SENSOR_DS_CAL_POINT1_REF;
  const float dsRaw2[SENSOR_MAXIMUM] = SENSOR_DS_CAL_POINT2_RAW;
  const float dsRef2[SENSOR_MAXIMUM] = SENSOR_DS_CAL_POINT2_REF;
  for (int i = 0; i < SENSOR_MAXIMUM; i++) {
    computeCalibration(dsRaw1[i], dsRef1[i], dsRaw2[i], dsRef2[i], dsCalSlope[i], dsCalOffset[i]);
  }
  computeCalibration(SENSOR_PT100X_CAL_POINT1_RAW, SENSOR_PT100X_CAL_POINT1_REF,
                      SENSOR_PT100X_CAL_POINT2_RAW, SENSOR_PT100X_CAL_POINT2_REF,
                      pt100xCalSlope, pt100xCalOffset);
  computeCalibration(SENSOR_BME680_CAL_POINT1_RAW, SENSOR_BME680_CAL_POINT1_REF,
                      SENSOR_BME680_CAL_POINT2_RAW, SENSOR_BME680_CAL_POINT2_REF,
                      bme680CalSlope, bme680CalOffset);
}

int setup_temp_sensors() {
  sensorlist.begin();
  sensorlist.setResolution(SENSOR_RESOLUTION);
  numberOfDevices = sensorlist.getDeviceCount();
  String output = "(";

  if (sensorlist.isParasitePowerMode()) output += "parasite:ON, ";
  for (int i = 0; i < numberOfDevices; i++) {
    if (sensorlist.getAddress(tempDeviceAddress, i)) {
      output += String(i) + "=DS18B20,";
      memcpy(sensorAddresses[i], tempDeviceAddress, sizeof(DeviceAddress));
    } else {
      output += String(i) + "=GHOST,";
    }
  }

  pt100x.begin(SENSOR_PT100X_Config);
  if (pt100x.temperature(SENSOR_PT100X_R_NOM, SENSOR_PT100X_R_REF) > PT100X_FAULT_THRESHOLD) {
    pt100x_found = true;
    output += String(numberOfDevices++) + "=PT100X,";
  }

  if (bme680.begin()) {
    bme680_found = true;
    bme680.setTemperatureOversampling(BME68X_OS_8X);
    bme680.setHumidityOversampling(BME68X_OS_2X);
    bme680.setPressureOversampling(BME68X_OS_4X);
    bme680.setIIRFilterSize(BME68X_FILTER_SIZE_3);
    bme680.setGasHeater(320, 150);
    output += String(numberOfDevices++) + "=BME680";
  } else {
#if SERIAL_ENABLE
    Serial.println("BME680 not found");
#endif
  }

  output += ")";
  syslog.log(LOG_INFO, output);
#if SERIAL_ENABLE
  Serial.println(output);
#endif
  return numberOfDevices;
}

void temperature_read() {
  int ds_count = numberOfDevices - (pt100x_found ? 1 : 0) - (bme680_found ? 1 : 0);
  int idx = ds_count;

  sensorlist.requestTemperatures();
  for (int i = 0; i < ds_count; i++) {
    float reading = sensorlist.getTempCByIndex(i);
    if (reading != DEVICE_DISCONNECTED_C) {
      temperatures[i] = reading * dsCalSlope[i] + dsCalOffset[i];
      if (i == PID_SENSOR_INDEX) lastSensorReadTime = millis();
    }
  }

  if (pt100x_found) {
    float reading = pt100x.temperature(SENSOR_PT100X_R_NOM, SENSOR_PT100X_R_REF);
    if (reading > PT100X_FAULT_THRESHOLD) {
      temperatures[idx] = reading * pt100xCalSlope + pt100xCalOffset;
      if (idx == PID_SENSOR_INDEX) lastSensorReadTime = millis();
    }
    idx++;
  }
  if (bme680_found && bme680.performReading()) {
    temperatures[idx] = bme680.temperature * bme680CalSlope + bme680CalOffset;
    if (idx == PID_SENSOR_INDEX) lastSensorReadTime = millis();
    idx++;
  }

  if (numberOfDevices > PID_SENSOR_INDEX)
    PID_Input = temperatures[PID_SENSOR_INDEX];
}

// ─── MQTT ─────────────────────────────────────────────────────────────────────

void subscribe_topics() {
  mqttClient.subscribe(TOPIC_INDUCTION_SET);
  mqttClient.subscribe(TOPIC_INDUCTION_CAP);
  mqttClient.subscribe(TOPIC_PID_WILDCARD);
  mqttClient.subscribe(TOPIC_TIMER_SET);
  mqttClient.subscribe(TOPIC_TIMER_CTL);
  mqttClient.subscribe(TOPIC_DEVICE_MODE);
  mqttClient.subscribe(TOPIC_RELAY_SET);
  mqttClient.subscribe(TOPIC_GPIO5_SET);
}

void setup_induction() {
  pinMode(INDUCTION_PIN_WHITE,  OUTPUT); digitalWrite(INDUCTION_PIN_WHITE, LOW);
  pinMode(INDUCTION_PIN_YELLOW, OUTPUT); digitalWrite(INDUCTION_PIN_YELLOW, HIGH);
  pinMode(INDUCTION_PIN_BLUE,   INPUT_PULLUP);
  // inductionCooker is already default-constructed at static-init time; no need to
  // reassign it here — doing so raced the ISR against the copy-assignment of the very
  // object it reads (attachInterrupt below can fire before the reassignment completes).
  attachInterrupt(digitalPinToInterrupt(INDUCTION_PIN_BLUE), readInputWrap, CHANGE);
}

void setup_pid() {
  PID_Input = PID_Setpoint = 0;
  myPID.SetMode(AUTOMATIC);
  myPID.SetOutputLimits(Output_Limit_Min, Output_Limit_Max);
  myPID.SetTunings(PID_P, PID_I, PID_D);
}

// ─── Network config persistence (LittleFS) ───────────────────────────────────
// Broker address, MQTT topic prefix, and OTA password, provisioned via the WiFiManager
// portal's custom parameters. Loaded before Wi-Fi/MQTT/OTA are set up (no syslog yet at
// that point, hence no logging here — a missing/corrupt file just falls back silently
// to the config.h defaults already assigned to these globals above).
#define NETCONFIG_FILE "/netconfig.json"

void saveNetConfig() {
  JsonDocument doc;
  doc["broker"]      = cfgMqttBroker;
  doc["mqttRoot"]    = cfgMqttRoot;
  doc["mqttDevice"]  = cfgMqttDevice;
  doc["otaPassword"] = cfgOtaPassword;
  File f = LittleFS.open(NETCONFIG_FILE, "w");
  if (!f) return;
  serializeJson(doc, f);
  f.close();
}

void loadNetConfig() {
  File f = LittleFS.open(NETCONFIG_FILE, "r");
  if (!f) return;  // first boot, or never saved — keep config.h defaults
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err != DeserializationError::Ok) return;
  if (doc["broker"].is<const char*>())      strlcpy(cfgMqttBroker,  doc["broker"],      sizeof(cfgMqttBroker));
  if (doc["mqttRoot"].is<const char*>())    strlcpy(cfgMqttRoot,    doc["mqttRoot"],    sizeof(cfgMqttRoot));
  if (doc["mqttDevice"].is<const char*>())  strlcpy(cfgMqttDevice,  doc["mqttDevice"],  sizeof(cfgMqttDevice));
  if (doc["otaPassword"].is<const char*>()) strlcpy(cfgOtaPassword, doc["otaPassword"], sizeof(cfgOtaPassword));
}

// Set by WiFiManager's save-config callback when the portal form is submitted; setup()
// checks this right after autoConnect()/startConfigPortal() to know whether to copy the
// custom parameters' values back into cfgMqtt*/cfgOtaPassword and persist them.
bool shouldSaveNetConfig = false;
void onNetConfigSaved() { shouldSaveNetConfig = true; }

// ─── Settings persistence (LittleFS) ─────────────────────────────────────────
// Runtime-tunable values (power cap, PID state/tunings/setpoint, device mode) live
// only in RAM otherwise, so a reboot silently reverts them to the config.h compile-time
// defaults. Persisted as a small JSON file, written whenever one of these values
// changes via MQTT and re-applied at boot, after setup_pid()'s own initialization.
#define SETTINGS_FILE "/settings.json"

void saveSettings() {
  JsonDocument doc;
  doc["cap"]      = powerCap;
  doc["mode"]     = deviceMode;
  doc["pidState"] = PID_state;
  doc["P"]        = PID_P;
  doc["I"]        = PID_I;
  doc["D"]        = PID_D;
  doc["setpoint"] = PID_Setpoint;
  File f = LittleFS.open(SETTINGS_FILE, "w");
  if (!f) { syslog.log(LOG_ERR, "Failed to open settings file for write"); return; }
  serializeJson(doc, f);
  f.close();
}

void loadSettings() {
  File f = LittleFS.open(SETTINGS_FILE, "r");
  if (!f) return;  // first boot, or never saved — keep config.h defaults
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err != DeserializationError::Ok) {
    syslog.log(LOG_WARNING, "Settings file corrupt, using defaults");
    return;
  }
  if (doc["cap"].is<int>())    powerCap   = constrain((int)doc["cap"], 0, 100);
  if (doc["mode"].is<int>())   deviceMode = ((int)doc["mode"] == MODE_SLAVE) ? MODE_SLAVE : MODE_STANDALONE;
  if (doc["pidState"].is<bool>()) PID_state = (bool)doc["pidState"];
  if (doc["P"].is<double>()) { PID_P = constrain((double)doc["P"], 0.0, (double)PID_MAX_P); pidTuningsDirty = true; }
  if (doc["I"].is<double>()) { PID_I = constrain((double)doc["I"], 0.0, (double)PID_MAX_I); pidTuningsDirty = true; }
  if (doc["D"].is<double>()) { PID_D = constrain((double)doc["D"], 0.0, (double)PID_MAX_D); pidTuningsDirty = true; }
  if (doc["setpoint"].is<double>())
    PID_Setpoint = constrain((double)doc["setpoint"], 0.0, (double)PID_SETPOINT_MAX);
  syslog.log(LOG_INFO, "Settings loaded from flash");
}

// Reconnect to MQTT and re-subscribe without resetting device state.
// Uses millis()-based timing so OTA stays responsive during reconnect.
void reconnect_mqtt() {
  syslog.log(LOG_WARNING, "MQTT disconnected, reconnecting...");
  int attempts = 0;
  unsigned long lastAttempt = 0;

  while (!mqttClient.connected()) {
    unsigned long now = millis();
    if (now - lastAttempt >= MQTT_RECONNECT_INTERVAL_MS) {
      lastAttempt = now;
      mqttClient.connect(HOSTNAME, BROKER_USER, BROKER_PASSWORD);
      char buf[32];
      snprintf(buf, sizeof(buf), "MQTT reconnect #%d", ++attempts);
      display_writex(display, 2, buf, true);
      if (attempts > MQTT_RECONNECT_MAX_ATTEMPTS) {
        syslog.log(LOG_ERR, "MQTT reconnect failed, rebooting");
        ESP.restart();
      }
    }
    ArduinoOTA.handle();  // stay responsive to OTA during reconnect
  }

  subscribe_topics();
  syslog.log(LOG_INFO, "MQTT reconnected");
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  JsonDocument doc;
  if (deserializeJson(doc, payload, length) != DeserializationError::Ok) return;

  bool settingsDirty = false;  // set true below when a persisted setting changes

  // induction/set — {power: 0-100}; 0 = off. Also accepts legacy {state:"off", power:N}
  if (strcmp(topic, TOPIC_INDUCTION_SET) == 0) {
    int pwr = doc["power"] | 0;
    String st = doc["state"] | String("on");
    inductionCooker.newPower = (st == "off" || pwr == 0) ? 0 : constrain(pwr, 0, powerCap);
    inductionCooker.Update();
    setLED(inductionCooker.power);
    display_update();
    message_received = true;
  }

  // induction/cap — {cap: 0-100}
  if (strcmp(topic, TOPIC_INDUCTION_CAP) == 0 && doc["cap"].is<int>()) {
    powerCap = constrain((int)doc["cap"], 0, 100);
    settingsDirty = true;
  }

  // pid/setpoint — settable regardless of PID state (but not in slave mode, see below)
  if (strcmp(topic, TOPIC_PID_SETPOINT) == 0 && doc["setpoint"].is<double>()) {
    PID_Setpoint = constrain((double)doc["setpoint"], 0.0, (double)PID_SETPOINT_MAX);
    settingsDirty = true;
  }

  if (deviceMode == MODE_STANDALONE) {
    // pid/enable — {enabled: true/false}. Gated on standalone mode: otherwise this could
    // set a latent PID_state=true while in slave mode, which would spring to life the
    // moment device/mode later switches back to standalone.
    if (strcmp(topic, TOPIC_PID_ENABLE) == 0) {
      PID_state = (bool)doc["enabled"];
      settingsDirty = true;
    }
    // PID_v1 recomputes Output from its internal outputSum on every Compute(), so just
    // zeroing Output snaps right back — toggling MANUAL/AUTOMATIC forces Initialize(),
    // which reseeds outputSum from Output (0) and lastInput from the current PID_Input.
    if (strcmp(topic, TOPIC_PID_RESET) == 0) {
      Output = 0;
      myPID.SetMode(MANUAL);
      myPID.SetMode(AUTOMATIC);
    }
    if (strcmp(topic, TOPIC_PID_P) == 0 && doc["P"].is<double>()) { PID_P = constrain((double)doc["P"], 0.0, (double)PID_MAX_P); pidTuningsDirty = settingsDirty = true; }
    if (strcmp(topic, TOPIC_PID_I) == 0 && doc["I"].is<double>()) { PID_I = constrain((double)doc["I"], 0.0, (double)PID_MAX_I); pidTuningsDirty = settingsDirty = true; }
    if (strcmp(topic, TOPIC_PID_D) == 0 && doc["D"].is<double>()) { PID_D = constrain((double)doc["D"], 0.0, (double)PID_MAX_D); pidTuningsDirty = settingsDirty = true; }
  }

  // timer/set — {duration: seconds}
  if (strcmp(topic, TOPIC_TIMER_SET) == 0) {
    brewTimerSet(brewTimer, (unsigned long)doc["duration"] * 1000UL);
  }

  // timer/ctl — {cmd: "start"/"pause"/"reset"}
  if (strcmp(topic, TOPIC_TIMER_CTL) == 0) {
    String cmd = doc["cmd"];
    if (cmd == "start")       brewTimerStart(brewTimer, millis());
    else if (cmd == "pause")  brewTimerPause(brewTimer, millis());
    else if (cmd == "reset")  brewTimerReset(brewTimer);
  }

  // relay/set — {state: "on"/"off"}, optional {duration: seconds} for timed operation
  if (strcmp(topic, TOPIC_RELAY_SET) == 0) {
    String st = doc["state"] | String("off");
    bool on = (st == "on");
    unsigned long duration = (unsigned long)(doc["duration"] | 0);
    setRelay(on);
    relayTimerEndMs = (on && duration > 0) ? millis() + duration * 1000UL : 0;
    relay_write_mqtt();
  }

  // gpio5/set — {state: "on"/"off"}, optional {duration: seconds} for timed operation
  if (strcmp(topic, TOPIC_GPIO5_SET) == 0) {
    String st = doc["state"] | String("off");
    bool on = (st == "on");
    unsigned long duration = (unsigned long)(doc["duration"] | 0);
    setGPIO5(on);
    gpio5TimerEndMs = (on && duration > 0) ? millis() + duration * 1000UL : 0;
    gpio5_write_mqtt();
  }

  // device/mode — {mode: "standalone"/"slave"}
  if (strcmp(topic, TOPIC_DEVICE_MODE) == 0) {
    String mode = doc["mode"];
    if (mode == "slave") {
      deviceMode = MODE_SLAVE;
      PID_state  = false;
      settingsDirty = true;
      syslog.log(LOG_INFO, "Mode: slave");
    } else if (mode == "standalone") {
      deviceMode = MODE_STANDALONE;
      settingsDirty = true;
      syslog.log(LOG_INFO, "Mode: standalone");
    }
  }

  if (settingsDirty) saveSettings();
}

// ─── Publish ──────────────────────────────────────────────────────────────────

// Returns current Unix timestamp if NTP is synced, 0 otherwise
uint32_t ntpTimestamp() {
  time_t now; time(&now);
  return (now > 1000000000UL) ? (uint32_t)now : 0;
}

void temperature_write_mqtt() {
  char topic[64];
  uint32_t ts = ntpTimestamp();
  for (int i = 0; i < numberOfDevices; i++) {
    JsonDocument doc;
    doc["temp"] = temperatures[i];
    if (ts) doc["ts"] = ts;
    char message[64];
    serializeJson(doc, message);
    snprintf(topic, sizeof(topic), "%s%d", PUBLISH_TOPIC_SENSOR_PREFIX, i);
    mqttClient.publish(topic, message);
  }
}

void induction_write_mqtt() {
  JsonDocument doc;
  uint32_t ts = ntpTimestamp();
  doc["relay"]  = inductionCooker.isRelayon ? 1 : 0;
  doc["active"] = inductionCooker.isInduon  ? 1 : 0;
  doc["power"]  = inductionCooker.power;
  doc["level"]  = inductionCooker.CMD_CUR;
  doc["error"]  = inductionLastError;
  doc["cap"]    = powerCap;
  if (ts) doc["ts"] = ts;
  char message[160];
  serializeJson(doc, message);
  mqttClient.publish(PUBLISH_TOPIC_INDUCTION, message);
}

void pid_write_mqtt() {
  JsonDocument doc;
  uint32_t ts = ntpTimestamp();
  doc["enabled"]  = PID_state  ? 1 : 0;
  doc["mode"]     = deviceModeName(deviceMode == MODE_SLAVE);
  doc["setpoint"] = PID_Setpoint;
  doc["input"]    = PID_Input;
  doc["output"]   = Output;
  doc["diff"]     = PID_Input - PID_Setpoint;
  doc["P"]        = PID_P;
  doc["I"]        = PID_I;
  doc["D"]        = PID_D;
  if (ts) doc["ts"] = ts;
  char message[224];
  serializeJson(doc, message);
  mqttClient.publish(PUBLISH_TOPIC_PID, message);
}

void timer_write_mqtt() {
  JsonDocument doc;
  uint32_t ts = ntpTimestamp();
  doc["running"]   = brewTimer.running ? 1 : 0;
  doc["paused"]    = brewTimer.paused  ? 1 : 0;
  doc["remaining"] = timerRemainingMs()      / 1000UL;
  doc["duration"]  = brewTimer.durationMs    / 1000UL;
  if (ts) doc["ts"] = ts;
  char message[96];
  serializeJson(doc, message);
  mqttClient.publish(PUBLISH_TOPIC_TIMER, message);
}

void relay_write_mqtt() {
  JsonDocument doc;
  uint32_t ts = ntpTimestamp();
  doc["state"] = relayState ? 1 : 0;
  doc["remaining"] = (relayTimerEndMs > 0 && !deadlineReached(millis(), relayTimerEndMs))
                     ? (relayTimerEndMs - millis()) / 1000UL : 0;
  if (ts) doc["ts"] = ts;
  char message[96];
  serializeJson(doc, message);
  mqttClient.publish(PUBLISH_TOPIC_RELAY, message);
}

void gpio5_write_mqtt() {
  JsonDocument doc;
  uint32_t ts = ntpTimestamp();
  doc["state"] = gpio5State ? 1 : 0;
  doc["remaining"] = (gpio5TimerEndMs > 0 && !deadlineReached(millis(), gpio5TimerEndMs))
                     ? (gpio5TimerEndMs - millis()) / 1000UL : 0;
  if (ts) doc["ts"] = ts;
  char message[96];
  serializeJson(doc, message);
  mqttClient.publish(PUBLISH_TOPIC_GPIO5, message);
}

void device_write_mqtt() {
  JsonDocument doc;
  uint32_t ts = ntpTimestamp();
  doc["uptime"]  = millis() / 1000UL;
  doc["heap"]    = ESP.getFreeHeap();
  doc["rssi"]    = WiFi.RSSI();
  doc["version"] = VERSION;
  doc["mode"]    = deviceModeName(deviceMode == MODE_SLAVE);
  if (ts) doc["ts"] = ts;
  char message[160];
  serializeJson(doc, message);
  mqttClient.publish(PUBLISH_TOPIC_DEVICE, message);
}

void publishStatus() {
  setLED(inductionCooker.power);
#if SERIAL_ENABLE
  Serial.println("T:" + String(temperatures[0], 1) + " Indu:" + String(inductionCooker.power) + "% PID:" + String(PID_state));
#endif
#if MQTT_ENABLE
  temperature_write_mqtt();
  induction_write_mqtt();
  pid_write_mqtt();
  timer_write_mqtt();
  relay_write_mqtt();
  gpio5_write_mqtt();
  device_write_mqtt();
#endif
}

// ─── Button ───────────────────────────────────────────────────────────────────

void handleButton(int val) {
  int bucket = getButtonBucket(val, BUTTON_THRESHOLD_B1, BUTTON_THRESHOLD_B2,
                                BUTTON_THRESHOLD_B3, BUTTON_THRESHOLD_B4, BUTTON_THRESHOLD_B5);
  if (bucket == 0) { lastButtonBucket = 0; return; }
  if (bucket == lastButtonBucket) return;
  if (millis() - lastButtonChange < BUTTON_DEBOUNCE_MS) return;
  lastButtonBucket = bucket;
  lastButtonChange = millis();
  const int powerMap[] = { 0, BUTTON_POWER_B1, BUTTON_POWER_B2, BUTTON_POWER_B3,
                               BUTTON_POWER_B4, BUTTON_POWER_B5, BUTTON_POWER_B6 };
  inductionCooker.newPower = constrain(powerMap[bucket], 0, powerCap);
  inductionCooker.Update();
  if (mqttClient.connected()) induction_write_mqtt();
#if SERIAL_ENABLE
  Serial.println("Button: " + String(powerMap[bucket]) + "%");
#endif
}

// ─── Web dashboard + browser OTA ───────────────────────────────────────────────
// Minimal built-in WebServer (no extra lib_deps — WebServer.h/Update.h ship with the
// arduino-esp32 core, same as ArduinoOTA/WiFi). Read-only status page plus a browser
// firmware-upload form, both gated behind the same OTA password ArduinoOTA already uses.

const char WEB_PAGE_HEAD[] PROGMEM =
    "<!DOCTYPE html><html><head><meta http-equiv='refresh' content='5'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>" HOSTNAME "</title>"
    "<style>body{font-family:sans-serif;background:#111;color:#eee;padding:1em}"
    "table{border-collapse:collapse}td{padding:2px 12px 2px 0}a{color:#6cf;text-decoration:none}"
    ".nav{display:flex;align-items:baseline;flex-wrap:wrap;margin-bottom:1em}"
    ".nav .brand{font-weight:bold;color:#eee;margin-right:1em}"
    ".nav .brand .ver{color:#888;font-weight:normal;font-size:0.85em;margin-left:6px}"
    ".nav .brand .prerelease{color:#111;background:#e8b400;font-weight:bold;font-size:0.75em;"
    "margin-left:6px;padding:1px 6px;border-radius:3px}"
    ".nav a{display:inline-block;padding:6px 14px;margin-right:4px;background:#222;border-radius:4px 4px 0 0}"
    ".nav a.active{background:#6cf;color:#111;font-weight:bold}"
    "</style></head><body>";

// Shared header + nav bar; `active` is "" / "update" / "config" to highlight the current tab.
// The status page auto-refreshes (meta refresh above) — that's harmless on /update and /config
// too since neither holds any unsaved form state worth losing every 5s.
String webPageHeader(const char *active) {
  String html = FPSTR(WEB_PAGE_HEAD);
  html += "<div class='nav'>";
  html += "<span class='brand'>" HOSTNAME "<span class='ver'>v" VERSION "</span>";
  if (isPrereleaseVersion(VERSION)) {
    html += "<span class='prerelease' title='Pre-release firmware — not a stable build'>&#9888; pre-release</span>";
  }
  html += "</span>";
  html += "<a href='/'"          + String(strcmp(active, "")       == 0 ? " class='active'" : "") + ">Status</a>";
  html += "<a href='/update'"    + String(strcmp(active, "update") == 0 ? " class='active'" : "") + ">Update</a>";
  html += "<a href='/config'"    + String(strcmp(active, "config") == 0 ? " class='active'" : "") + ">Config</a>";
  html += "</div>";
  return html;
}

void handleWebRoot() {
  String html = webPageHeader("");
  html += "<h2>" HOSTNAME "</h2><table>";
  for (int i = 0; i < numberOfDevices; i++) {
    html += "<tr><td>Sensor " + String(i) + "</td><td>" + String(temperatures[i], 1) + " &deg;C</td></tr>";
  }
  html += "<tr><td>Setpoint</td><td>" + String(PID_Setpoint, 1) + " &deg;C</td></tr>";
  html += "<tr><td>PID</td><td>" + String(PID_state ? "on" : "off") + "</td></tr>";
  html += "<tr><td>Mode</td><td>" + String(deviceModeName(deviceMode == MODE_SLAVE)) + "</td></tr>";
  html += "<tr><td>Induction power</td><td>" + String(inductionCooker.power) + " % (cap " + String(powerCap) + " %)</td></tr>";
  html += "<tr><td>Relay</td><td>" + String(inductionCooker.isRelayon ? "on" : "off") + "</td></tr>";
  html += "<tr><td>GPIO5</td><td>" + String(gpio5State ? "on" : "off") + "</td></tr>";
  if (brewTimer.running) {
    html += "<tr><td>Brew timer</td><td>" + String(timerRemainingMs() / 1000UL) + " s remaining</td></tr>";
  } else {
    html += "<tr><td>Brew timer</td><td>stopped</td></tr>";
  }
  html += "<tr><td>WiFi RSSI</td><td>" + String(WiFi.RSSI()) + " dBm</td></tr>";
  html += "<tr><td>Uptime</td><td>" + String(millis() / 1000UL) + " s</td></tr>";
  html += "<tr><td>Free heap</td><td>" + String(ESP.getFreeHeap()) + " B</td></tr>";
  html += "<tr><td>Version</td><td>" VERSION "</td></tr>";
  html += "</table></body></html>";
  webServer.send(200, "text/html", html);
}

// GitHub repo backing the release-check link/button below. v3 releases use "3.*" tags
// (kept separate from V1's plain "vX.Y.Z" tags) — see CHANGELOG.md.
#define GITHUB_REPO_URL "https://github.com/Gr3yh0und/brewingstation"

void handleWebUpdatePage() {
  if (!webServer.authenticate(hostname, cfgOtaPassword)) return webServer.requestAuthentication();
  String html = webPageHeader("update");
  html += "<h2>Firmware update</h2>"
          "<form method='POST' action='/update' enctype='multipart/form-data'>"
          "<input type='file' name='firmware' accept='.bin'> "
          "<input type='submit' value='Upload'></form>"
          "<p><a href='" GITHUB_REPO_URL "/releases' target='_blank' rel='noopener'>View releases on GitHub</a></p>"
          "<p><button onclick='checkUpdate()'>Check for update</button> <span id='updateResult'></span></p>"
          // Runs entirely in the browser (fetches api.github.com directly) rather than on
          // the device — avoids needing a TLS stack/root CA on the ESP32 just to check a
          // version string. Requires the *browser* to have internet access; the device's
          // own LAN connectivity is irrelevant to this check.
          "<script>"
          "var CURRENT_VERSION='" VERSION "';"
          "function checkUpdate(){"
            "var r=document.getElementById('updateResult');"
            "r.textContent='Checking...';"
            "fetch('https://api.github.com/repos/Gr3yh0und/brewingstation/releases')"
            ".then(function(res){return res.json();})"
            ".then(function(list){"
              "var v3=list.find(function(rel){return rel.tag_name.indexOf('3.')===0;});"
              "if(!v3){r.textContent='No v3 release found on GitHub.';return;}"
              "if(v3.tag_name===CURRENT_VERSION){"
                "r.textContent='Up to date ('+CURRENT_VERSION+').';"
              "}else{"
                "r.innerHTML='Update available: '+v3.tag_name+' (running '+CURRENT_VERSION+') "
                  "&mdash; <a href=\"'+v3.html_url+'\" target=\"_blank\" rel=\"noopener\">view release</a>';"
              "}"
            "})"
            ".catch(function(){r.textContent='Check failed (browser has no internet access, or GitHub is unreachable).';});"
          "}"
          "</script>"
          "</body></html>";
  webServer.send(200, "text/html", html);
}

// Streams the uploaded .bin straight into the inactive OTA partition via the Update
// library — same mechanism ArduinoOTA uses, just fed from an HTTP multipart body
// instead of the network OTA protocol.
void handleWebUpdateUpload() {
  HTTPUpload &upload = webServer.upload();
  if (upload.status == UPLOAD_FILE_START) {
    webUpdateFailed = false;
    display_writex(display, 2, "OTA (web): updating...", true);
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      webUpdateFailed = true;
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE && !webUpdateFailed) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      webUpdateFailed = true;
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END && !webUpdateFailed) {
    if (!Update.end(true)) {
      webUpdateFailed = true;
      Update.printError(Serial);
    }
  }
}

void handleWebUpdateResult() {
  if (!webServer.authenticate(hostname, cfgOtaPassword)) return webServer.requestAuthentication();
  bool ok = !webUpdateFailed && !Update.hasError();
  String html = webPageHeader("update");
  html += ok ? "<h2>Update OK, rebooting&hellip;</h2>" : "<h2>Update failed</h2>";
  html += "</body></html>";
  webServer.send(200, "text/html", html);
  if (ok) {
    display_writex(display, 2, "OTA (web): done, reboot", true);
    delay(500);
    ESP.restart();
  } else {
    display_writex(display, 2, "OTA (web): failed", true);
  }
}

// Builds the current runtime config as JSON — shared by the human-readable /config page
// (OTA password masked there) and the raw /config/download attachment (unmasked, since
// downloading it is a deliberate backup action already gated behind the same auth).
void buildConfigJson(JsonDocument &doc) {
  doc["hostname"]    = HOSTNAME;
  doc["version"]     = VERSION;
  doc["wifiSsid"]    = WiFi.SSID();
  doc["ipAddress"]   = WiFi.localIP().toString();
  doc["mqttBroker"]  = cfgMqttBroker;
  doc["mqttRoot"]    = cfgMqttRoot;
  doc["mqttDevice"]  = cfgMqttDevice;
  doc["otaPassword"] = cfgOtaPassword;
  doc["powerCap"]    = powerCap;
  doc["deviceMode"]  = deviceModeName(deviceMode == MODE_SLAVE);
  doc["pidEnabled"]  = PID_state;
  doc["pidSetpoint"] = PID_Setpoint;
  doc["pidP"]        = PID_P;
  doc["pidI"]        = PID_I;
  doc["pidD"]        = PID_D;
}

void handleWebConfigPage() {
  if (!webServer.authenticate(hostname, cfgOtaPassword)) return webServer.requestAuthentication();
  JsonDocument doc;
  buildConfigJson(doc);
  String html = webPageHeader("config");
  html += "<h2>Current configuration</h2><table>";
  html += "<tr><td>Hostname</td><td>" HOSTNAME "</td></tr>";
  html += "<tr><td>Firmware version</td><td>" VERSION "</td></tr>";
  html += "<tr><td>WiFi SSID</td><td>" + String(doc["wifiSsid"].as<const char*>()) + "</td></tr>";
  html += "<tr><td>IP address</td><td>" + String(doc["ipAddress"].as<const char*>()) + "</td></tr>";
  html += "<tr><td>MQTT broker</td><td>" + String(cfgMqttBroker) + "</td></tr>";
  html += "<tr><td>MQTT topic prefix</td><td>" + String(cfgMqttRoot) + "/" + String(cfgMqttDevice) + "</td></tr>";
  html += "<tr><td>OTA password</td><td>&bull;&bull;&bull;&bull;&bull;&bull;&bull;&bull; (see download for plaintext)</td></tr>";
  html += "<tr><td>Power cap</td><td>" + String(powerCap) + " %</td></tr>";
  html += "<tr><td>Device mode</td><td>" + String(deviceModeName(deviceMode == MODE_SLAVE)) + "</td></tr>";
  html += "<tr><td>PID enabled</td><td>" + String(PID_state ? "yes" : "no") + "</td></tr>";
  html += "<tr><td>PID setpoint</td><td>" + String(PID_Setpoint, 1) + " &deg;C</td></tr>";
  html += "<tr><td>PID tunings (P/I/D)</td><td>" + String(PID_P, 4) + " / " + String(PID_I, 4) + " / " + String(PID_D, 4) + "</td></tr>";
  html += "</table><p><a href='/config/download'>Download config (.json)</a></p></body></html>";
  webServer.send(200, "text/html", html);
}

void handleWebConfigDownload() {
  if (!webServer.authenticate(hostname, cfgOtaPassword)) return webServer.requestAuthentication();
  JsonDocument doc;
  buildConfigJson(doc);
  String json;
  serializeJsonPretty(doc, json);
  webServer.sendHeader("Content-Disposition", "attachment; filename=\"" HOSTNAME "-config.json\"");
  webServer.send(200, "application/json", json);
}

void setup_web_server() {
  webServer.on("/", HTTP_GET, handleWebRoot);
  webServer.on("/update", HTTP_GET, handleWebUpdatePage);
  webServer.on("/update", HTTP_POST, handleWebUpdateResult, handleWebUpdateUpload);
  webServer.on("/config", HTTP_GET, handleWebConfigPage);
  webServer.on("/config/download", HTTP_GET, handleWebConfigDownload);
  webServer.begin();
}

// Timer callback wrappers — arduino-timer v2.3+ requires bool(void*) signature
static bool _cb_temperature_read(void*)  { temperature_read();  return true; }
static bool _cb_publishStatus(void*)     { publishStatus();     return true; }
static bool _cb_handleInduction(void*)   { handleInduction();   return true; }
static bool _cb_display_update(void*)    { display_update();    return true; }

// ─── Setup ────────────────────────────────────────────────────────────────────

void setup() {
#if SERIAL_ENABLE
  Serial.begin(SERIAL_BAUDRATE);
  Serial.println("Brewing Station " VERSION);
#endif

  const esp_task_wdt_config_t wdt_cfg = { .timeout_ms = WDT_TIMEOUT_S * 1000U, .idle_core_mask = 0, .trigger_panic = true };
  esp_task_wdt_init(&wdt_cfg);
  esp_task_wdt_add(NULL);

  // true = format on mount failure (e.g. first boot, or a previously non-LittleFS partition).
  // Runs before Wi-Fi/syslog are up, so failures here are silent beyond the return value —
  // loadSettings()/loadNetConfig() below simply find no file and fall back to config.h defaults.
  LittleFS.begin(true);
  loadNetConfig();  // must run before the Wi-Fi block below, which needs cfgMqttBroker etc.

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  setup_display(display,  0x3C);
  setup_display(display2, 0x3D);
  display_writex(display2, 0, "v" VERSION, false);
  delay(300);
  display.clearDisplay();

  pinMode(BUTTON_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT); digitalWrite(RELAY_PIN, LOW);
  pinMode(GPIO_EXT_PIN, OUTPUT); digitalWrite(GPIO_EXT_PIN, LOW);
  setup_buzzer();

  // PCF8574 LED expander — all outputs HIGH (LEDs off, open-drain active-LOW)
  if (!ledExpander.begin()) {
    Serial.println("PCF8574 not found — check I2C address and wiring");
  }
  setAllLEDs(false);

  // Boot-time button hold — gives a ~2s window to force the WiFiManager config portal
  // open even if saved Wi-Fi credentials would still work, so the device can be
  // reprovisioned (new broker/topic-prefix/OTA password too) without clearing flash.
  display_writex(display, 0, "Hold button for Wi-Fi setup...", false);
  bool forcePortal = false;
  unsigned long holdStart = millis();
  while (millis() - holdStart < 2000) {
    if (getButtonBucket(analogRead(BUTTON_PIN), BUTTON_THRESHOLD_B1, BUTTON_THRESHOLD_B2,
                         BUTTON_THRESHOLD_B3, BUTTON_THRESHOLD_B4, BUTTON_THRESHOLD_B5) != 0) {
      forcePortal = true;
      break;
    }
    delay(50);
  }

  // Wi-Fi — WiFiManager: uses ESP32's saved NVS credentials if present, otherwise (or on
  // failure, or a forced portal above) opens a captive-portal AP so the device can be
  // provisioned without a reflash. Custom fields let the same portal also set the MQTT
  // broker/topic-prefix/OTA password, pre-filled from netconfig.json / config.h defaults.
  display_writex(display, 0, forcePortal ? "Wi-Fi setup..." : "Wi-Fi...", false);
  WiFi.setHostname(hostname);

  WiFiManagerParameter p_broker("broker", "MQTT broker IP/host", cfgMqttBroker, sizeof(cfgMqttBroker));
  WiFiManagerParameter p_root("root", "MQTT root path", cfgMqttRoot, sizeof(cfgMqttRoot));
  WiFiManagerParameter p_device("device", "MQTT device name", cfgMqttDevice, sizeof(cfgMqttDevice));
  WiFiManagerParameter p_ota("ota", "OTA password", cfgOtaPassword, sizeof(cfgOtaPassword));
  wm.addParameter(&p_broker);
  wm.addParameter(&p_root);
  wm.addParameter(&p_device);
  wm.addParameter(&p_ota);
  wm.setSaveConfigCallback(onNetConfigSaved);
  wm.setConfigPortalTimeout(180);
  wm.setAPCallback([](WiFiManager *mgr) {
    display_writex(display, 1, "AP: " + mgr->getConfigPortalSSID(), false);
    display_writex(display, 2, "IP: 192.168.4.1", false);
  });

  bool connected = forcePortal ? wm.startConfigPortal(hostname) : wm.autoConnect(hostname);
  if (!connected) {
    display_writex(display, 0, "Wi-Fi: failed, reboot", true);
    delay(5000);
    ESP.restart();
  }

  if (shouldSaveNetConfig) {
    strlcpy(cfgMqttBroker,  p_broker.getValue(), sizeof(cfgMqttBroker));
    strlcpy(cfgMqttRoot,    p_root.getValue(),   sizeof(cfgMqttRoot));
    strlcpy(cfgMqttDevice,  p_device.getValue(), sizeof(cfgMqttDevice));
    strlcpy(cfgOtaPassword, p_ota.getValue(),    sizeof(cfgOtaPassword));
    saveNetConfig();
  }

  display_writex(display, 0, "Wi-Fi: " + WiFi.localIP().toString(), false);
  configTzTime(NTP_TIMEZONE, NTP_SERVER);
#if SERIAL_ENABLE
  Serial.println("Wi-Fi: " + WiFi.localIP().toString());
#endif

  // mDNS
  if (!MDNS.begin(HOSTNAME)) {
    display_writex(display, 1, "mDNS: failed", false);
  } else {
    display_writex(display, 1, "mDNS: ok", false);
  }

  // OTA
  ArduinoOTA.setPort(OTA_UPDATE_PORT);
  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.setPassword(cfgOtaPassword);
  ArduinoOTA.onStart([]() {
    display_writex(display, 2, "OTA: updating...", true);
  });
  ArduinoOTA.onEnd([]() {
    display_writex(display, 2, "OTA: done", true);
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    display_writex(display, 2, "OTA: " + String(progress / (total / 100)) + "%", true);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    display_writex(display, 2, "OTA: error " + String(error), true);
  });
  ArduinoOTA.begin();
  display_writex(display, 2, "OTA: ready", false);

  // Web dashboard + browser-based OTA upload (http://<ip>/, http://<ip>/update)
  setup_web_server();

  // MQTT
  display_writex(display, 3, "MQTT...", false);
  mqttClient.setServer(cfgMqttBroker, BROKER_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.connect(HOSTNAME, BROKER_USER, BROKER_PASSWORD);
  int counter = 0;
  while (!mqttClient.connected()) {
    delay(100);
    if (++counter > 50) {
      syslog.log(LOG_ERR, "No MQTT connection at startup, rebooting");
      display_writex(display, 3, "MQTT: failed, reboot", true);
      delay(1000);
      ESP.restart();
    }
  }
  display_writex(display, 3, "MQTT: ok", false);

  // Pre-compute publish- and subscribe-side topic strings (done once to avoid per-call heap
  // allocations) from the runtime root/device — provisionable via the portal, so these can
  // no longer be pasted together by the preprocessor.
  snprintf(PUBLISH_TOPIC_SENSOR_PREFIX, sizeof(PUBLISH_TOPIC_SENSOR_PREFIX),
           "%s/%s/%s/", cfgMqttRoot, cfgMqttDevice, SENSOR_MQTT_PREFIX);
  snprintf(PUBLISH_TOPIC_INDUCTION, sizeof(PUBLISH_TOPIC_INDUCTION),
           "%s/%s/%s", cfgMqttRoot, cfgMqttDevice, INDUCTION_MQTT_STATUS);
  snprintf(PUBLISH_TOPIC_PID, sizeof(PUBLISH_TOPIC_PID),
           "%s/%s/%s", cfgMqttRoot, cfgMqttDevice, PID_MQTT_TOPIC);
  snprintf(PUBLISH_TOPIC_TIMER, sizeof(PUBLISH_TOPIC_TIMER),
           "%s/%s/timer", cfgMqttRoot, cfgMqttDevice);
  snprintf(PUBLISH_TOPIC_DEVICE, sizeof(PUBLISH_TOPIC_DEVICE),
           "%s/%s/device", cfgMqttRoot, cfgMqttDevice);
  snprintf(PUBLISH_TOPIC_RELAY, sizeof(PUBLISH_TOPIC_RELAY),
           "%s/%s/relay", cfgMqttRoot, cfgMqttDevice);
  snprintf(PUBLISH_TOPIC_GPIO5, sizeof(PUBLISH_TOPIC_GPIO5),
           "%s/%s/gpio5", cfgMqttRoot, cfgMqttDevice);
  computeSubscribeTopics();

  // Induction cooker
  display_writex(display, 4, "Cooker...", false);
  setup_induction();
  ledBootAnimation();
  display_writex(display, 4, "Cooker: ok", false);

  // Sensors
  display_writex(display, 5, "Sensors...", false);
  setup_sensor_calibration();
  counter = 0;
  while (setup_temp_sensors() == 0 && counter++ < 3) {
#if SERIAL_ENABLE
    Serial.println("No sensors found, retrying...");
#endif
    delay(1000);
  }
  lastSensorReadTime = millis();  // grace period before staleness check triggers
  display_writex(display, 5, "Sensors: " + String(numberOfDevices), false);

  // PID
  setup_pid();

  // Subscribe MQTT topics (called once here; reconnect_mqtt() re-subscribes on reconnect)
  subscribe_topics();

  // Timers
  timerTempRead.every(FREQUENCY_READTEMP, _cb_temperature_read);
#if PUBLISH_ENABLE
  timerTempStatus.every(FREQUENCY_STATUS, _cb_publishStatus);
#endif
  timerInductionStatus.every(FREQUENCY_INDUCTION, _cb_handleInduction);
  timerDisplayUpdate.every(DISPLAY_FREQUENCY, _cb_display_update);

  syslog.log(LOG_INFO, "Startup ok (v" VERSION ")");
#if SERIAL_ENABLE
  Serial.println("Setup done.");
#endif
  delay(1500);
}

// ─── Loop ─────────────────────────────────────────────────────────────────────

void loop() {
  esp_task_wdt_reset();

  handleButton(analogRead(BUTTON_PIN));

  ArduinoOTA.handle();
  webServer.handleClient();

  mqttClient.loop();
  if (!mqttClient.connected()) reconnect_mqtt();

  timerTempRead.tick();
  timerTempStatus.tick();
  timerInductionStatus.tick();
  timerDisplayUpdate.tick();

  checkTimerExpiry();

  // Safety: sensor staleness — fires whenever the cooker is on, regardless of mode
  if (inductionCooker.isInduon && !isSensorHealthy()) {
    char buf[48];
    snprintf(buf, sizeof(buf), "primary sensor stale >%lus", SENSOR_STALE_TIMEOUT_MS / 1000UL);
    safetyShutdown(buf);
  }

  // Safety: thermal runaway — standalone only; slave mode relies on the cooker's own E3 protection
  if (deviceMode == MODE_STANDALONE && PID_state
      && isThermalRunaway(PID_Input, PID_Setpoint, (double)PID_SAFETY_OVERSHOOT)) {
    char buf[48];
    snprintf(buf, sizeof(buf), "thermal runaway T=%.1f setpoint=%.1f", PID_Input, PID_Setpoint);
    safetyShutdown(buf);
  }

  // Induction error: read status frame from cooker RX interrupt
  {
    uint8_t err = inductionCooker.getError();
    if (err != inductionLastError) {
      inductionLastError = err;
      char msg[56];
      if (err == 0) {
        syslog.log(LOG_INFO, "Induction: error cleared");
      } else {
        snprintf(msg, sizeof(msg), "Induction error: %s (code %u)", inductionErrorString(err), err);
        syslog.log(LOG_WARNING, msg);
        if (err == 4 || err == 5) safetyShutdown("induction E3 overheat");
        if (err == 9)             safetyShutdown("induction E7 low voltage");
        if (err == 10)            safetyShutdown("induction E8 high voltage");
      }
    }
  }

  // Relay timer — auto-off when duration expires
  if (relayTimerEndMs > 0 && deadlineReached(millis(), relayTimerEndMs)) {
    setRelay(false);
    relayTimerEndMs = 0;
    if (mqttClient.connected()) relay_write_mqtt();
  }

  // GPIO5 timer — auto-off when duration expires
  if (gpio5TimerEndMs > 0 && deadlineReached(millis(), gpio5TimerEndMs)) {
    setGPIO5(false);
    gpio5TimerEndMs = 0;
    if (mqttClient.connected()) gpio5_write_mqtt();
  }

  // PID control — standalone mode only
  if (deviceMode == MODE_STANDALONE && PID_state) {
    if (pidTuningsDirty) {
      myPID.SetTunings(PID_P, PID_I, PID_D);
      pidTuningsDirty = false;
    }
    myPID.Compute();
    inductionCooker.newPower = constrain((int)Output, 0, powerCap);
    inductionCooker.Update();
    setLED(inductionCooker.power);
  }
}

