// Project for an automated beer brewing station with the purpose to automatically hold a certain temperature in a beer kettle based on temperature sensors and by controlling an induction cooker
// Remains compatible to CraftBeerPi 3 and 4
// Induction Cooker Code based on the work of Innuendo: https://github.com/InnuendoPi/MQTTDevice2
// Used devices: DS18B20 within metal/silicon tubes, GGM IDS2 induction cooker (via serial connection), SSD1306 OLED displays
// Michael Morscher, April 2023
// Tested on Arduino IDE 2.0.4 / VSCode
// Board: DOIT ESP32 DEVKIT V1
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <SPI.h>

// Additional used libraries
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Syslog.h>
#include "timer.h"
#include "timerManager.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_MAX31865.h>
#include <Adafruit_BME280.h>
#include <PID_v1.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSans24pt7b.h>
#include "config.h"

#define VERSION 1.13

// Feature Toggles
#define SERIAL_ENABLE false
#define MQTT_ENABLE true
#define PUBLISH_ENABLE true

#define SENSOR_MAXIMUM 5  // Total amount of possible sensors
#define TEMPERATURE_MQTT_STATUS "temperature"

// Sensors & Displays
OneWire oneWire(SENSOR_BUS_PIN);
DallasTemperature sensorlist(&oneWire);
Adafruit_MAX31865 pt100x = Adafruit_MAX31865(SENSOR_PT100X_CS_PIN, SENSOR_PT100X_DI_PIN, SENSOR_PT100X_DO_PIN, SENSOR_PT100X_CLK_PIN);  // Software SPI
bool pt100x_found = false;
Adafruit_BME280 bme280 = Adafruit_BME280(SENSOR_BME280_CS_PIN, SENSOR_BME280_DI_PIN, SENSOR_BME280_DO_PIN, SENSOR_BME280_CLK_PIN);  // Software SPI
bool bme280_found = false;
Adafruit_SSD1306 display(DISPLAY_SCREEN_WIDTH, DISPLAY_SCREEN_HEIGHT, &Wire, DISPLAY_OLED_RESET);
Adafruit_SSD1306 display2(DISPLAY_SCREEN_WIDTH, DISPLAY_SCREEN_HEIGHT, &Wire, DISPLAY_OLED_RESET);
// Wifi
const uint8_t bitmap26[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0x80, 0x07, 0xff, 0xe0, 0x1f, 0x81, 0xf8, 0x7c, 0x00, 0x3e, 0xf0, 0x00, 0x0f, 0xe0, 0xff, 0x07, 0x03, 0xff, 0xc0, 0x0f, 0xe7, 0xf0, 0x0f, 0x00, 0xf0, 0x08, 0x00, 0x10, 0x00, 0x7e, 0x00, 0x00, 0xff, 0x00, 0x00, 0xe7, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
// Incoming MQTT message
const uint8_t bitmap27[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x03, 0x18, 0xc0, 0x03, 0x99, 0x80, 0x01, 0xdb, 0x80, 0x00, 0xff, 0x00, 0x00, 0x7e, 0x00, 0x30, 0x3c, 0x0c, 0x30, 0x18, 0x0c, 0x30, 0x00, 0x0c, 0x30, 0x00, 0x0c, 0x30, 0x00, 0x0c, 0x38, 0x00, 0x1c, 0x3f, 0xff, 0xfc, 0x1f, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
bool display_toggle = true;

// Button resistor ladder
#define BUTTON_PIN 34
// Define the resistor values for each button
const int R1 = 10000;
const int R2 = 20000;
const int R3 = 30000;
const int R4 = 40000;
const int R5 = 50000;
const int R6 = 60000;

// Induction Cooker
const int SIGNAL_HIGH = 5120;
const int SIGNAL_HIGH_TOL = 1500;
const int SIGNAL_LOW = 1280;
const int SIGNAL_LOW_TOL = 500;
const int SIGNAL_START = 25;
const int SIGNAL_START_TOL = 10;
const int SIGNAL_WAIT = 10;
const int SIGNAL_WAIT_TOL = 5;
unsigned char PWR_STEPS[6] = { 0, 20, 40, 60, 80, 100 };  // Power steps in percentage between states
unsigned char PWR_LEDS[6] = { INDUCTION_LED_0_PIN, INDUCTION_LED_20_PIN, INDUCTION_LED_40_PIN, INDUCTION_LED_60_PIN, INDUCTION_LED_80_PIN, INDUCTION_LED_100_PIN };
String errorMessages[10] = { "E0", "E1", "E2", "E3", "E4", "E5", "E6", "E7", "E8", "EC" };

// PID controller
#define PID_MQTT_TOPIC "pid"
double PID_Setpoint;
double PID_Input;
double Output;
double Output_Limit_Max = 100;
double Output_Limit_Min = 0;
double PID_P = 45.27;
double PID_I = 0.2371;
double PID_D = 7.2;
bool PID_state = false;
PID myPID(&PID_Input, &Output, &PID_Setpoint, PID_P, PID_I, PID_D, P_ON_M, DIRECT);  //P_ON_M specifies that Proportional on Measurement be used
                                                                                     //P_ON_E (Proportional on Error) is the default behavior

// Network settings
const char *ssid = SSID_NAME;
const char *password = SSID_PASSWORD;
const char *hostname = HOSTNAME;
WiFiClient wifiClient;
WiFiUDP udpClient;
PubSubClient mqttClient(wifiClient);
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, hostname, SYSLOG_APP_NAME, LOG_KERN);
bool message_received = false;

// Timer
Timer timerTempStatus;
Timer timerTempRead;
Timer timerInductionStatus;
Timer timerDisplayUpdate;

// Temperature sensor variables
int numberOfDevices;                           // Number of temperature devices found
DeviceAddress tempDeviceAddress;               // Temporary storage for a devices address
DeviceAddress sensorAdresses[SENSOR_MAXIMUM];  // List of all found sensor addresses
float temperatures[SENSOR_MAXIMUM];
float temperature_combined;

class induction {
  unsigned long timeTurnedoff;
  long timeOutCommand = 5000;   // TimeOut for serial commands
  long timeOutReaction = 2000;  // TimeOut for serial communication
  unsigned long lastInterrupt;
  unsigned long lastCommand;
  bool inputStarted = false;
  unsigned char inputCurrent = 0;
  unsigned char inputBuffer[33];
  bool isError = false;
  unsigned char error = 0;
  long powerSampletime = 20000;
  unsigned long powerLast;
  long powerHigh = powerSampletime;  // Time of "HIGH" part within control cycle
  long powerLow = 0;

  // Binary signals for induction cooker
  int CMD[6][33] = {
    { 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0 },  // Off
    { 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0 },  // P1 state
    { 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0 },  // P2 state
    { 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0 },  // P3 state
    { 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0 },  // P4 state
    { 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0 }
  };  // P5 state

public:
  unsigned char PIN_WHITE = INDUCTION_PIN_WHITE;     // Relais control
  unsigned char PIN_YELLOW = INDUCTION_PIN_YELLOW;   // TX to induction
  unsigned char PIN_INTERRUPT = INDUCTION_PIN_BLUE;  // Interrupt from induction
  int power = 0;
  int newPower = 0;
  unsigned char CMD_CUR = 0;  // Current command
  boolean isRelayon = false;  // System state: Is the relay activated?
  boolean isInduon = false;   // System state: Is the power > 0?
  boolean isPower = false;
  long delayAfteroff = INDUCTION_FAN_DELAY;
  int powerLevelOnError = 100;    // 100% schaltet das Event handling für Induktion aus
  int powerLevelBeforeError = 0;  // in error event save last power state
  bool induction_state = true;    // Error state induction

  // Constructor
  induction() {
    // Replace 1/0 from fixed commands with time value for HIGH and LOW
    for (int i = 0; i < 33; i++) {
      for (int j = 0; j < 6; j++) {
        if (CMD[j][i] == 1) {
          CMD[j][i] = SIGNAL_HIGH;
        } else {
          CMD[j][i] = SIGNAL_LOW;
        }
      }
    }
  }

  // Helper function for waiting time
  void millis2wait(const int &value) {
    delay(value);
  }

  // Update current state of induction cooker
  void Update() {
    updatePower();

    isRelayon = updateRelay();

    if (isInduon && power > 0) {
      if (millis() > powerLast + powerSampletime) {
        powerLast = millis();
      }
      if (millis() > powerLast + powerHigh) {
        sendCommand(CMD[CMD_CUR - 1]);
        isPower = false;
      } else {
        sendCommand(CMD[CMD_CUR]);
        isPower = true;
      }
    } else if (isRelayon) {
      sendCommand(CMD[0]);
    }
  }

  // Update current state of the relay
  bool updateRelay() {
    // If power is requested, but relay is off: Turn relay on
    if (isInduon == true && isRelayon == false) {
      digitalWrite(PIN_WHITE, HIGH);
      return true;
    }

    // If power is not requested anymore, delay for fans has expired and relay is still on: Turn relay off
    if (isInduon == false && isRelayon == true) {
      if (millis() > timeTurnedoff + delayAfteroff) {
        digitalWrite(PIN_WHITE, LOW);
        return false;
      }
    }

    // If power is not requested and relay is already off: Keep relay off
    if (isInduon == false && isRelayon == false) {
      return false;
    }

    // If relay is on, keep it on
    return true;
  }

  // Update current state (power) of induction cooker
  void updatePower() {
    unsigned long currentTime = millis();

    // Update power if a new command has been received
    if (power != newPower) {
      // Limit newPower to the range 0-100
      newPower = min(100, max(0, newPower));

      // Update power and reset variables
      power = newPower;
      timeTurnedoff = 0;
      isInduon = true;
      long difference = 0;

      // If power is 0, turn off and exit
      if (power == 0) {
        CMD_CUR = 0;
        timeTurnedoff = currentTime;
        isInduon = false;
        difference = 0;
        powerLow = 0;
        powerHigh = 0;
        return;
      }

      // Find the appropriate CMD_CUR value and calculate the difference
      for (int i = 1; i < 7; i++) {
        if (power <= PWR_STEPS[i]) {
          CMD_CUR = i;
          difference = PWR_STEPS[i] - power;
          break;
        }
      }

      // Calculate powerLow and powerHigh based on difference
      if (difference != 0) {
        powerLow = powerSampletime * difference / 20L;
        powerHigh = powerSampletime - powerLow;
      } else {
        powerHigh = powerSampletime;
        powerLow = 0;
      }
    }
  }

  // Send new command to induction cooker via serial connection
  void sendCommand(int command[33]) {
    digitalWrite(PIN_YELLOW, HIGH);
    millis2wait(SIGNAL_START);
    digitalWrite(PIN_YELLOW, LOW);
    millis2wait(SIGNAL_WAIT);
    for (int i = 0; i < 33; i++) {
      digitalWrite(PIN_YELLOW, HIGH);
      delayMicroseconds(command[i]);
      digitalWrite(PIN_YELLOW, LOW);
      delayMicroseconds(SIGNAL_LOW);
    }
  }

  // Read information from induction cooker for error handling
  void readInput() {
    bool ishigh = digitalRead(PIN_INTERRUPT);
    unsigned long newInterrupt = micros();
    long signalTime = newInterrupt - lastInterrupt;

    // Filter jitter/flitch from serial line
    if (signalTime > 10) {
      if (ishigh) {
        lastInterrupt = newInterrupt;  // PIN signal is rising, sending of bit has started
      } else {                         // Pin signal is Falling, bit transmission done - now evaluate results

        if (!inputStarted) {  // search for starting bit
          if (signalTime < 35000L && signalTime > 15000L) {
            inputStarted = true;
            inputCurrent = 0;
          }
        } else {                    // Hat Begonnen. Nehme auf.
          if (inputCurrent < 34) {  // nur bis 33 aufnehmen.
            if (signalTime < (SIGNAL_HIGH + SIGNAL_HIGH_TOL) && signalTime > (SIGNAL_HIGH - SIGNAL_HIGH_TOL)) {
              // HIGH BIT erkannt
              inputBuffer[inputCurrent] = 1;
              inputCurrent += 1;
            }
            if (signalTime < (SIGNAL_LOW + SIGNAL_LOW_TOL) && signalTime > (SIGNAL_LOW - SIGNAL_LOW_TOL)) {
              // LOW BIT erkannt
              inputBuffer[inputCurrent] = 0;
              inputCurrent += 1;
            }
          } else {  // Aufnahme vorbei.

            /* Auswerten */
            //newError = BtoI(13, 4); // Fehlercode auslesen.

            /* von Vorne */
            //timeLastReaction = millis();
            //inputCurrent = 0;
            //inputStarted = false;
          }
        }
      }
    }
  }

  // Error conversion
  unsigned long BtoI(int start, int numofbits) {  //binary array to integer conversion
    unsigned long integer = 0;
    unsigned long mask = 1;
    for (int i = numofbits + start - 1; i >= start; i--) {
      if (inputBuffer[i])
        integer |= mask;
      mask = mask << 1;
    }
    return integer;
  }
};

// Induction cooker Instantiation
induction inductionCooker;

// UNUSED: but used for external interrupts to induction cooker
void readInputWrap() {
  inductionCooker.readInput();
}

// Update induction cooker status, used for timer handling
void handleInduction() {
  inductionCooker.Update();
  display_update();
  setLED(inductionCooker.power);
}

// Setup SSD1306 OLED I2C display
void setup_display(Adafruit_SSD1306 &display, unsigned int address) {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Serial.print("Setting up display ...");
  if (!display.begin(SSD1306_SWITCHCAPVCC, address)) {
    Serial.println(F("Allocation for display failed"));
  }
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setFont(&FreeSans24pt7b);
  display.setCursor(29, 46);
  display.println("OK");
  display.display();
  Serial.println(" Done!");
}

// Setup One Wire temperature sensors
int setup_temp_sensors() {
  // DS18B20

  // Start sensor configuration and count sensors
  sensorlist.begin();
  sensorlist.setResolution(SENSOR_RESOLUTION);
  numberOfDevices = sensorlist.getDeviceCount();
  String output = "(";

  // Report parasite power requirements and identified sensors
  if (sensorlist.isParasitePowerMode()) {
    output += "parasite power is: ON, ";
  }

  // Loop through each device, print out address
  for (int i = 0; i < numberOfDevices; i++) {
    // Search the wire for address of healthy sensors
    if (sensorlist.getAddress(tempDeviceAddress, i)) {
      output = output + i + "=DS18B20,";
      memcpy(sensorAdresses[i], tempDeviceAddress, sizeof(DeviceAddress));
    }
    // if not healthy report ghost sensors
    else {
      output = output + i + "=GHOST,";
      // ToDo: report error
    }
  }

  // PT100X
  pt100x.begin(SENSOR_PT100X_Config);
  float reading = pt100x.temperature(SENSOR_PT100X_R_NOM, SENSOR_PT100X_R_REF);
  if (reading != float(-242.02)) {
    pt100x_found = true;
    output = output + numberOfDevices + "=PT100X,";
    numberOfDevices = numberOfDevices + 1;
  }

  // BME280
  unsigned status;
  status = bme280.begin();
  if (!status) {
    Serial.println("Could not find a valid BME280 sensor, check wiring, address, sensor ID!");
    Serial.print("SensorID was: 0x");
    Serial.println(bme280.sensorID(), 16);
    Serial.print("        ID of 0xFF probably means a bad address, a BMP 180 or BMP 085\n");
    Serial.print("   ID of 0x56-0x58 represents a BMP 280,\n");
    Serial.print("        ID of 0x60 represents a BME 280.\n");
    Serial.print("        ID of 0x61 represents a BME 680.\n");
  } else {
    bme280_found = true;
    output = output + numberOfDevices + "=BME280";
    numberOfDevices = numberOfDevices + 1;
  }

  output += "))... ";
  syslog.log(LOG_INFO, output);
  Serial.print(output);
  return numberOfDevices;
}

// Setup GGM IDS2 induction cooker
void setup_induction() {
  // Configure pins for serial connection
  pinMode(INDUCTION_PIN_WHITE, OUTPUT);
  digitalWrite(INDUCTION_PIN_WHITE, LOW);
  pinMode(INDUCTION_PIN_YELLOW, OUTPUT);
  digitalWrite(INDUCTION_PIN_YELLOW, HIGH);
  pinMode(INDUCTION_PIN_BLUE, INPUT_PULLUP);  // Interrupt
  //attachInterrupt(digitalPinToInterrupt(INDUCTION_PIN_BLUE), readInputWrap, CHANGE);

  inductionCooker = induction();

  // Subscribe to MQTT topic for control
  String topic_string = String(MQTT_ROOT_PATH) + "/" + String(MQTT_DEVICE) + "/" + String(INDUCTION_MQTT_COMMANDS);
  char topic[50] = {};
  topic_string.toCharArray(topic, topic_string.length() + 1);
  mqttClient.subscribe(topic);
}

// Setup PID controller
void setup_pid() {
  // Initialize the variables we're linked to
  PID_Input = 0;
  PID_Setpoint = 0;

  // Turn the PID on
  myPID.SetMode(AUTOMATIC);
  myPID.SetOutputLimits(Output_Limit_Min, Output_Limit_Max);

  // Subscribe to MQTT topic for control
  String topic_string = String(MQTT_ROOT_PATH) + "/" + String(MQTT_DEVICE) + "/" + String(PID_MQTT_TOPIC) + "/#";
  char topic[50] = {};
  topic_string.toCharArray(topic, topic_string.length() + 1);
  mqttClient.subscribe(topic);
}

// Callback whenever new MQTT message on a subscribed topic is received
void mqttCallback(char *topic, byte *payload, unsigned int length) {

  // Interprete incoming JSON message
  StaticJsonDocument<256> doc;
  deserializeJson(doc, payload, length);

  // Induction control
  if (strcmp(topic, "cave/brewery/heater/power") == 0) {
    String state = doc["state"];
    int power = doc["power"];
    if (state == "off") {
      inductionCooker.newPower = 0;
      Serial.println("Induction: Changing power to 0");
    } else {
      inductionCooker.newPower = power;
      Serial.print("Induction: Changing power to ");
      Serial.println(power);
    }

    // Trigger setting of new values at induction cooker
    inductionCooker.Update();

    // Update LED and Display
    setLED(inductionCooker.power);
    display_update();
  }

  // PID: Turn on / off
  if (strcmp(topic, "cave/brewery/pid/control") == 0) {
    String state = doc["state"];
    if (state == "on") {
      PID_state = true;
      Serial.println("PID: Turning on PID controller!");
    } else {
      PID_state = false;
      Serial.println("PID: Turning off PID controller!");
    }
  }

  if (PID_state == true) {
    // PID: Reset Output
    if (strcmp(topic, "cave/brewery/pid/reset") == 0) {
      String output = doc["output"];
      if (output == "true") {
        Output = 0;
        Serial.println("PID: Reset of PID Output to 0 successfull!");
      } else {
        Serial.println("PID: Reset parameter wrong!");
      }
    }

    // PID: Change P proportion
    if (strcmp(topic, "cave/brewery/pid/p") == 0) {
      double P_prop = doc["P"];
      if (P_prop != PID_P) {
        Serial.print("PID: Changing P proportion from ");
        Serial.print(PID_P);
        Serial.print(" to ");
        Serial.println(P_prop);
        PID_P = P_prop;
      }
    }

    // PID: Change I proportion
    if (strcmp(topic, "cave/brewery/pid/i") == 0) {
      double I_prop = doc["I"];
      if (I_prop != PID_I) {
        Serial.print("PID: Changing I proportion from ");
        Serial.print(PID_I);
        Serial.print(" to ");
        Serial.println(I_prop);
        PID_I = I_prop;
      }
    }

    // PID: Change D proportion
    if (strcmp(topic, "cave/brewery/pid/d") == 0) {
      double D_prop = doc["D"];
      if (D_prop != PID_D) {
        Serial.print("PID: Changing D proportion from ");
        Serial.print(PID_D);
        Serial.print(" to ");
        Serial.println(D_prop);
        PID_D = D_prop;
      }
    }

    // PID: Set Target Temperature
    if (strcmp(topic, "cave/brewery/pid/target") == 0) {
      double targetTemperature = doc["targetTemperature"];
      PID_Setpoint = targetTemperature;
      Serial.println("PID: Setting target temperature for PID to " + String(targetTemperature));
    }
  }

  // Debugging
  if (strcmp(topic, "cave/brewery/pid") == 1) {
    String output = "MQTT: Received on topic [" + String(topic) + "] ";
    for (int i = 0; i < length; i++) {
      output += (char)payload[i];
    }
    Serial.println(output);
    message_received = true;
  }
}

// General Setup Phase
void setup() {
  // Serial configuration
  Serial.begin(SERIAL_BAUDRATE);
  Serial.print("Project version: ");
  Serial.println(VERSION);

  // Setup display
  setup_display(display, 0x3C);
  setup_display(display2, 0x3D);
  display_writex(display2, 0, String(VERSION), false);
  delay(300);
  display.clearDisplay();

  // Setup buttons
  pinMode(BUTTON_PIN, INPUT);

  // WIFI configuration
  Serial.print("Setting up Wifi connection... ");
  display_writex(display, 0, "Wi-Fi:", false);
  WiFi.mode(WIFI_STA);
  WiFi.hostname(hostname);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Wi-Fi connection failed! Rebooting...");
    display_writex(display, 1, "Wi-Fi: Failed - Reboot", false);
    delay(5000);
    ESP.restart();
  }
  Serial.println("Done! (IP=" + WiFi.localIP().toString() + ")");
  display_writex(display, 0, "Wi-Fi: " + WiFi.localIP().toString(), false);

  // mDNS configuration
  Serial.print("Setting up mDNS connection... ");
  display_writex(display, 1, "MDNS:", false);
  if (!MDNS.begin(HOSTNAME)) {
    Serial.println("Error setting up MDNS responder!");
    display_writex(display, 1, "MDNS: Failed", true);
  }
  Serial.println("Done!");
  display_writex(display, 1, "MDNS: Started", false);

  // OTA configuration
  Serial.print("Setting up OTA connection... ");
  ArduinoOTA.setPort(OTA_UPDATE_PORT);
  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else  // U_SPIFFS
      type = "filesystem";
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("OTA: Start updating " + type);
    display_writex(display, 2, "OTA: Start updating...", true);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA End");
    display_writex(display, 2, "OTA: Ended", true);
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
    display_writex(display, 2, "OTA: Progress...", true);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)
      Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)
      Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR)
      Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR)
      Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR)
      Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Done!");

  // MQTT configuration
  Serial.print("Setting up MQTT broker connection... ");
  display_writex(display, 2, "MQTT:", false);
  mqttClient.setServer(BROKER_ADDRESS, BROKER_PORT);
  mqttClient.connect(HOSTNAME, BROKER_USER, BROKER_PASSWORD);
  mqttClient.setCallback(mqttCallback);
  int counter = 0;
  while (!mqttClient.connected()) {
    if (counter > 30) {
      syslog.log(LOG_INFO, "Startup not successfull, no connection to MQTT host could be established!");
      display_writex(display, 2, "MQTT: Failed, Retry", true);
    }
    delay(100);
    counter++;
  }
  Serial.println("Done!");
  display_writex(display, 2, "MQTT: Started", false);

  // Induction Cooker configuration
  Serial.print("Setting up Induction cooker... ");
  display_writex(display, 3, "Cooker", false);
  setup_induction();
  display_writex(display, 3, "Cooker      : Started", false);
  Serial.println("Done!");

  // Sensor configuration
  Serial.print("Setting up Sensors... ");
  display_writex(display, 4, "Sensors: ", false);
  counter = 0;
  while ((setup_temp_sensors() == 0) & (counter < 3)) {
    Serial.println("Error: No sensors found!");
    counter++;
    delay(1000);
  }
  display_writex(display, 4, "Sensors: " + String(numberOfDevices), false);
  Serial.println("Done!");

  // Setup and Test Induction Status LEDs
  Serial.print("Setting up LEDs... ");
  pinMode(INDUCTION_LED_0_PIN, OUTPUT);
  pinMode(INDUCTION_LED_20_PIN, OUTPUT);
  pinMode(INDUCTION_LED_40_PIN, OUTPUT);
  pinMode(INDUCTION_LED_60_PIN, OUTPUT);
  pinMode(INDUCTION_LED_80_PIN, OUTPUT);
  pinMode(INDUCTION_LED_100_PIN, OUTPUT);
  testLEDs();
  Serial.println("Done!");
  display_writex(display, 3, "Cooker + LED: Started", false);

  // PID configuration
  Serial.print("Setting up PID control... ");
  setup_pid();
  Serial.println("Done!");

  // Timer configuration
  Serial.print("Setting up timer control... ");
  timerTempRead.setInterval(FREQUENCY_READTEMP);
  timerTempRead.setCallback(temperature_read);
  if (PUBLISH_ENABLE) {
    timerTempStatus.setInterval(FREQUENCY_STATUS);
    timerTempStatus.setCallback(publishStatus);
  }
  timerInductionStatus.setInterval(FREQUENCY_INDUCTION);
  timerInductionStatus.setCallback(handleInduction);
  timerDisplayUpdate.setInterval(DISPLAY_FREQUENCY);
  timerDisplayUpdate.setCallback(display_update);
  TimerManager::instance().start();
  Serial.println("Done!");

  syslog.log(LOG_INFO, "Startup successfull! (Version: " + String(VERSION) + ")");
  Serial.print("Setup: Done!\n");
  delay(2000);
}

// Read out temperature of all sensors
void temperature_read() {
  int numberOfDevices_local = numberOfDevices;
  if (pt100x_found) {
    numberOfDevices_local--;
  }
  if (bme280_found) {
    numberOfDevices_local--;
  }

  // DS18B20 sensors
  sensorlist.requestTemperatures();
  for (int i = 0; i < numberOfDevices_local; i++) {
    float reading = sensorlist.getTempCByIndex(i);

    // buffer bad readings and show the old value instead
    if (reading != -127.00) {
      temperatures[i] = reading;
    }
  }

  // PT100X sensor
  if (pt100x_found) {
    float reading = pt100x.temperature(SENSOR_PT100X_R_NOM, SENSOR_PT100X_R_REF);
    temperatures[numberOfDevices_local] = reading;
    numberOfDevices_local++;
  }

  // BME280 sensor
  if (bme280_found) {
    float reading = bme280.readTemperature() - SENSOR_BME280_OFFSET;
    temperatures[numberOfDevices_local] = reading;
    numberOfDevices_local++;
  }
}

// Write current stored read out temperatures to serial output
void temperature_write_serial() {
  String output = "Temperature(";
  if (numberOfDevices == 0) {
    output += "No sensors found!";
  } else {
    for (int i = 0; i < numberOfDevices; i++) {
      output = output + "Sensor" + i + ":" + temperatures[i] + "°C,";
    }
  }
  Serial.println(output + ")");
}

// Write current stored read out temperatures to MQTT topic
void temperature_write_mqtt() {
  for (int i = 0; i < numberOfDevices; i++) {
    // Create JSON structure
    StaticJsonDocument<512> doc;
    //ToDo: doc["time"] = NOW();
    doc["temperature"] = temperatures[i];
    char message[512];
    serializeJson(doc, message);

    // Create topic character array and send information to MQTT topic
    String topic = String(MQTT_ROOT_PATH) + "/" + String(MQTT_DEVICE) + "/" + String(TEMPERATURE_MQTT_STATUS) + "/" + i;
    mqttClient.publish(topic.c_str(), message);
  }
}

// Write current state of induction cooker to serial output
void induction_write_serial() {
  String output = "Induction(isInduon=" + String(inductionCooker.isInduon) + ",isPower=" + inductionCooker.isPower + ",isRelayon=" + inductionCooker.isRelayon + ",power=" + inductionCooker.power + ",level=" + inductionCooker.CMD_CUR + ")";
  Serial.println(output);
}

// Write current state of induction cooker to MQTT topic
void induction_write_mqtt() {
  // Create JSON structure
  StaticJsonDocument<512> doc;
  //ToDo: doc["time"] = NOW();
  doc["relayOn"] = inductionCooker.isRelayon;
  doc["inductionOn"] = inductionCooker.isInduon;
  doc["powerPercent"] = inductionCooker.power;
  doc["powerLevel"] = inductionCooker.CMD_CUR;

  char message[512];
  serializeJson(doc, message);

  // Create topic character array
  String topic = String(MQTT_ROOT_PATH) + "/" + String(MQTT_DEVICE) + "/" + String(INDUCTION_MQTT_STATUS);

  // Publish information to MQTT topic
  mqttClient.publish(topic.c_str(), message);
}

// Write current state of pid control to serial output
void pid_write_serial() {
  String output = "PID(state=" + String(PID_state) + ",targetTemperature=" + PID_Setpoint + ",input=" + PID_Input + ",output=" + Output + ")";
  Serial.println(output);
}

// Write current state of PID control to MQTT topic
void pid_write_mqtt() {
  // Create JSON structure
  StaticJsonDocument<256> doc;

  // ToDo: Add timestamp to JSON structure
  doc["state"] = PID_state;                    // Add PID state to JSON structure
  doc["P"] = PID_P;                            // Add PID proportional gain to JSON structure
  doc["I"] = PID_I;                            // Add PID integral gain to JSON structure
  doc["D"] = PID_D;                            // Add PID derivative gain to JSON structure
  doc["input"] = PID_Input;                    // Add PID input to JSON structure
  doc["targetTemperature"] = PID_Setpoint;     // Add PID setpoint to JSON structure
  doc["tempDiff"] = PID_Input - PID_Setpoint;  // Add temperature difference to JSON structure
  doc["output"] = Output;                      // Add PID output to JSON structure

  char message[256];
  serializeJson(doc, message);

  // Create topic character array and send information to MQTT topic
  String topic = String(MQTT_ROOT_PATH) + "/" + String(MQTT_DEVICE) + "/" + String(PID_MQTT_TOPIC);
  mqttClient.publish(topic.c_str(), message);
}

// Get current runtime of the system
char *get_runtime() {
  // Get the current runtime in milliseconds
  unsigned long runtime_ms = millis();

  // Convert the runtime to minutes and seconds
  unsigned int runtime_m = runtime_ms / 60000;
  unsigned int runtime_s = (runtime_ms / 1000) % 60;

  // Create a string to hold the formatted runtime
  static char runtime_str[8];
  sprintf(runtime_str, "%03d:%02d", runtime_m, runtime_s);
  return runtime_str;
}

// Write the current information to the screen
void display_update() {

  char *runtime = get_runtime();

  // Display 1
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setFont(&FreeSans9pt7b);
  display.setCursor(0, 15);
  display.drawLine(0, 20, 102, 20, 1);
  display.drawLine(0, 42, 102, 42, 1);
  display.drawLine(102, 0, 102, 63, 1);
  display.println("Total");
  display.println("Left");
  display.println("Next");
  // Total
  display.setCursor(45, 15);
  display.println(runtime);
  // Now
  display.setCursor(45, 37);
  display.println("");
  // Next
  display.setCursor(45, 59);
  display.println("");
  // Wifi Status
  if (WiFi.status() == WL_CONNECTED) {
    display.drawBitmap(104, -3, bitmap26, 24, 24, 1);
  }
  // Activity Status
  if (display_toggle == true) {
    display_toggle = false;
  } else {
    display_toggle = true;
  }
  if (display_toggle == true) {
    display.fillCircle(116, 31, 6, 1);
  }
  // Message received
  if (message_received == true) {
    display.drawBitmap(104, 42, bitmap27, 24, 24, 1);
    message_received = false;
  }

  display.display();

  // Display 2
  display2.clearDisplay();
  display2.setTextColor(WHITE);
  display2.setTextSize(1);
  display2.setFont(&FreeSans18pt7b);
  display2.drawLine(0, 32, 128, 32, 1);
  display2.drawLine(70, 0, 70, 63, 1);
  // Temp Sensor 1
  display2.setCursor(0, 26);
  display2.println(String(temperatures[0], 1));
  // Temp Sensor 2
  display2.setCursor(0, 61);
  display2.println(String(temperatures[1], 1));
  // ToDo: Target Temperature
  //display2.setCursor(0, 61);
  //display2.println(String(temperatures[1], 1));
  // Induction Power Level
  display2.setCursor(91, 27);
  display2.println(String(inductionCooker.CMD_CUR));
  // Induction Power Percent
  if (inductionCooker.power == 100) {
    display2.setCursor(70, 61);
  } else if (inductionCooker.power < 10) {
    display2.setCursor(91, 61);
  } else {
    display2.setCursor(82, 61);
  }
  display2.println(String(inductionCooker.power));
  // FAN Display
  if (inductionCooker.isRelayon) {
    display2.drawCircle(80, 22, 8, 1);
    display2.fillTriangle(80, 20, 83, 17, 77, 17, 1);
    display2.fillTriangle(80, 24, 83, 27, 77, 27, 1);
    display2.fillTriangle(78, 22, 75, 19, 75, 25, 1);
    display2.fillTriangle(82, 22, 85, 19, 85, 25, 1);
  }
  // PID Display
  if (PID_state == true) {
    display2.setFont(NULL);
    display2.setCursor(73, 0);
    display2.setTextSize(1);
    display2.println("PID");
  }
  display2.display();
}

// Write single lines to a display with optional reset.
void display_writex(Adafruit_SSD1306 &display, int row, const String &thisIsAString, bool reset) {
  // Erase old content
  if (reset == true) {
    display.clearDisplay();
  }

  // Control display
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setFont(NULL);
  display.setCursor(0, row * 10);
  display.println(thisIsAString);

  // Print buffer on display
  display.display();
}

// Configure output LEDs based on power
void setLED(int value) {
  // Ensure value is between 0 and 100
  value = constrain(value, 0, 100);

  // Deactivate all LEDs
  digitalWrite(INDUCTION_LED_0_PIN, LOW);
  digitalWrite(INDUCTION_LED_20_PIN, LOW);
  digitalWrite(INDUCTION_LED_40_PIN, LOW);
  digitalWrite(INDUCTION_LED_60_PIN, LOW);
  digitalWrite(INDUCTION_LED_80_PIN, LOW);
  digitalWrite(INDUCTION_LED_100_PIN, LOW);

  // Activate corresponding LED
  if (value == 0) {
    digitalWrite(INDUCTION_LED_0_PIN, HIGH);
  } else if (value > 0 && value <= 20) {
    digitalWrite(INDUCTION_LED_20_PIN, HIGH);
  } else if (value > 20 && value <= 40) {
    digitalWrite(INDUCTION_LED_40_PIN, HIGH);
  } else if (value > 40 && value <= 60) {
    digitalWrite(INDUCTION_LED_60_PIN, HIGH);
  } else if (value > 60 && value <= 80) {
    digitalWrite(INDUCTION_LED_80_PIN, HIGH);
  } else {
    digitalWrite(INDUCTION_LED_100_PIN, HIGH);
  }
}

// Test LEDs
void testLEDs() {
  int ledPins[] = { INDUCTION_LED_0_PIN, INDUCTION_LED_20_PIN, INDUCTION_LED_40_PIN, INDUCTION_LED_60_PIN, INDUCTION_LED_80_PIN, INDUCTION_LED_100_PIN };
  int numLeds = sizeof(ledPins) / sizeof(int);
  for (int i = 0; i < numLeds; i++) {
    digitalWrite(ledPins[i], HIGH);
    delay(INDUCTION_LED_TEST_TIME);
    digitalWrite(ledPins[i], LOW);
  }
}

// Publish all states to outputs
void publishStatus() {
  setLED(inductionCooker.power);
  if (SERIAL_ENABLE) {
    temperature_write_serial();
    induction_write_serial();
    pid_write_serial();
  }
  if (MQTT_ENABLE) {
    temperature_write_mqtt();
    induction_write_mqtt();
    pid_write_mqtt();
  }
}

// Handle induction cooker buttons - Map voltage to buttons
void handleButton(int val) {
  if (val == 0) {
    Serial.println("Button 6 pressed - L5");
    inductionCooker.newPower = 100;
    inductionCooker.Update();
  } else if (val < 2000) {
    Serial.println("Button 5 pressed - L4");
    inductionCooker.newPower = 70;
    inductionCooker.Update();
  } else if (val < 2600) {
    Serial.println("Button 4 pressed - L3");
    inductionCooker.newPower = 50;
    inductionCooker.Update();
  } else if (val < 3000) {
    Serial.println("Button 3 pressed - L2");
    inductionCooker.newPower = 30;
    inductionCooker.Update();
  } else if (val < 3200) {
    Serial.println("Button 2 pressed - L1");
    inductionCooker.newPower = 10;
    inductionCooker.Update();
  } else if (val < 3300) {
    Serial.println("Button 1 pressed - L0");
    inductionCooker.newPower = 0;
    inductionCooker.Update();
  }

  // Send update to MQTT
  induction_write_mqtt();
}

// Main loop
void loop() {

  // Handle button
  int val = analogRead(BUTTON_PIN);
  handleButton(val);

  // Handle incoming OTA updates
  ArduinoOTA.handle();

  // Handle incoming MQTT mesages
  mqttClient.loop();
  if (!mqttClient.connected()) {
    setup();
  }

  // Update timers (read temp, update status, etc.)
  TimerManager::instance().update();

  // PID control
  if (PID_state == true) {
    myPID.SetTunings(PID_P, PID_I, PID_D);
    myPID.Compute();
    inductionCooker.newPower = Output;
    inductionCooker.Update();
    setLED(inductionCooker.power);
    display_update();
  }
}
