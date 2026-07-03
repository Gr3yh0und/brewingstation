// Project for an automated beer brewing station
// The purpose is to automatically hold a certain temperature in a beer kettle based on temperature sensors and by controlling an induction cooker
// Remains compatible to CraftBeerPi 3
// Based on the work of Innuendo: https://github.com/InnuendoPi/MQTTDevice2
// Used devices: DS18B20 within metal/silicon tubes, GGM IDS2 induction cooker (via serial connection), SSD1306 OLED displays
// Michael Morscher, July 2020
// Tested on Arduino IDE 1.8.12 / VSCode
// Board: NodeMCU ESP8266 "Wemos D1 mini"
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>

// Additional used libraries
// ArduinoOTA: https://github.com/esp8266/Arduino/tree/master/libraries/ArduinoOTA
// ESP8266mDNS: https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266mDNS
// pubsubclient: https://github.com/knolleary/pubsubclient - v2.8
// ArduinoJson: https://github.com/bblanchon/ArduinoJson - v6.15.2
// Syslog: https://github.com/arcao/Syslog - master from 10.9.2018
// Timer: https://github.com/brunocalou/Timer.git - Master
// OneWire: https://github.com/PaulStoffregen/OneWire - v2.3.5
// Arduino-Temperature-Control-Library: https://github.com/milesburton/Arduino-Temperature-Control-Library - v3.8.0
// Arduino-PID-Library: https://github.com/br3ttb/Arduino-PID-Library.git - v1.2.1
// ESP8266-OLED-SSD1306: https://github.com/squix78/esp8266-oled-ssd1306 - tested with v4.1.0
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Syslog.h>
#include "timer.h"
#include "timerManager.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <PID_v1.h>
#include "SSD1306Wire.h"

#define VERSION 1.0
#define SERIAL_BAUDRATE 115200

#define SERIAL_ENABLE false
#define MQTT_ENABLE true
#define PUBLISH_ENABLE true

// Network Configuration
#define HOSTNAME "ESP-BREWING"
#define SSID_NAME "your-ssid"
#define SSID_PASSWORD "your-password"
#define SERVER_ADDRESS "192.168.0.x"
#define OTA_UPDATE_PORT 8266
#define MQTT_ROOT_PATH "cave"
#define MQTT_DEVICE "brewery"

// Syslog server connection info
#define SYSLOG_SERVER SERVER_ADDRESS
#define SYSLOG_PORT 514
#define SYSLOG_APP_NAME HOSTNAME

// MQTT connection settings
#define BROKER_ADDRESS SERVER_ADDRESS
#define BROKER_PORT 1883
#define BROKER_USER NULL
#define BROKER_PASSWORD NULL

// SENSOR configuration
#define SENSOR_BUS_PIN D3        // Data bus pin
#define SENSOR_MAXIMUM 10         // Total amount of possible sensors
#define SENSOR_RESOLUTION 10          // SENSOR_RESOLUTION of DS18B20 sensors (9 - 12)
#define FREQUENCY_READTEMP 100 // Time how often temperature should be read
#define FREQUENCY_STATUS 800   // Time how often temperature status updates are sent
#define TEMPERATURE_MQTT_STATUS "temperature"
OneWire oneWire(SENSOR_BUS_PIN);
DallasTemperature sensorlist(&oneWire);

// DISPLAY configuration for both displays
#define FREQUENCY_DISPLAY 800
SSD1306Wire display(0x3c, SDA, SCL);   // ADDRESS, SDA, SCL  -  SDA and SCL usually populate automatically based on your board's pins_arduino.h
SSD1306Wire display2(0x3d, SDA, SCL);   // ADDRESS, SDA, SCL  -  SDA and SCL usually populate automatically based on your board's pins_arduino.h

// Induction cooker configuration
#define FREQUENCY_INDUCTION 800   // Interval to update induction cooker power
#define INDUCTION_PIN_WHITE D7    // Relais
#define INDUCTION_PIN_YELLOW D6   // TX to cooker
#define INDUCTION_PIN_BLUE D5     // Interrupt
#define INDUCTION_FAN_DELAY 60000 // Default time for cooling fans after power off (factory default = 120000)
#define INDUCTION_MQTT_STATUS "heater"
#define INDUCTION_MQTT_COMMANDS "heater/power"
const int SIGNAL_HIGH = 5120;
const int SIGNAL_HIGH_TOL = 1500;
const int SIGNAL_LOW = 1280;
const int SIGNAL_LOW_TOL = 500;
const int SIGNAL_START = 25;
const int SIGNAL_START_TOL = 10;
const int SIGNAL_WAIT = 10;
const int SIGNAL_WAIT_TOL = 5;
unsigned char PWR_STEPS[] = {0, 20, 40, 60, 80, 100}; // Power steps in percentage between states
String errorMessages[10] = {"E0", "E1", "E2", "E3", "E4", "E5", "E6", "E7", "E8", "EC"};

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
PID myPID(&PID_Input, &Output, &PID_Setpoint, PID_P, PID_I, PID_D, P_ON_M, DIRECT); //P_ON_M specifies that Proportional on Measurement be used
                                                                                    //P_ON_E (Proportional on Error) is the default behavior

// Network settings
const char *ssid = SSID_NAME;
const char *password = SSID_PASSWORD;
const char *hostname = HOSTNAME;
WiFiClient wifiClient;
WiFiUDP udpClient;
PubSubClient mqttClient(wifiClient);
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, hostname, SYSLOG_APP_NAME, LOG_KERN);

// Timer
Timer timerTempStatus;
Timer timerTempRead;
Timer timerInductionStatus;
Timer timerDisplayUpdate;

// Temperature sensor variables
int numberOfDevices;                       // Number of temperature devices found
DeviceAddress tempDeviceAddress;           // Temporary storage for a devices address
DeviceAddress sensorAdresses[SENSOR_MAXIMUM]; // List of all found sensor addresses
float temperatures[SENSOR_MAXIMUM];
float temperature_combined;

class induction
{
  unsigned long timeTurnedoff;
  long timeOutCommand = 5000;  // TimeOut for serial commands
  long timeOutReaction = 2000; // TimeOut for serial communication
  unsigned long lastInterrupt;
  unsigned long lastCommand;
  bool inputStarted = false;
  unsigned char inputCurrent = 0;
  unsigned char inputBuffer[33];
  bool isError = false;
  unsigned char error = 0;
  long powerSampletime = 20000;
  unsigned long powerLast;
  long powerHigh = powerSampletime; // Time of "HIGH" part within control cycle
  long powerLow = 0;

  // Binary signals for induction cooker
  int CMD[6][33] = {
      {1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0},  // Off
      {1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0},  // P1 state
      {1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0},  // P2 state
      {1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0},  // P3 state
      {1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0},  // P4 state
      {1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0}}; // P5 state

public:
  unsigned char PIN_WHITE = INDUCTION_PIN_WHITE;    // Relais control
  unsigned char PIN_YELLOW = INDUCTION_PIN_YELLOW;  // TX to induction
  unsigned char PIN_INTERRUPT = INDUCTION_PIN_BLUE; // Interrupt from induction
  int power = 0;
  int newPower = 0;
  unsigned char CMD_CUR = 0; // Current command
  boolean isRelayon = false; // System state: Is the relay activated?
  boolean isInduon = false;  // System state: Is the power > 0?
  boolean isPower = false;
  long delayAfteroff = INDUCTION_FAN_DELAY;
  int powerLevelOnError = 100;   // 100% schaltet das Event handling für Induktion aus
  int powerLevelBeforeError = 0; // in error event save last power state
  bool induction_state = true;   // Error state induction

  // Constructor
  induction()
  {
    // Replace 1/0 from fixed commands with time value for HIGH and LOW
    for (int i = 0; i < 33; i++)
    {
      for (int j = 0; j < 6; j++)
      {
        if (CMD[j][i] == 1)
        {
          CMD[j][i] = SIGNAL_HIGH;
        }
        else
        {
          CMD[j][i] = SIGNAL_LOW;
        }
      }
    }
  }

  // Helper function for waiting time
  void millis2wait(const int &value)
  {
    unsigned long pause = millis();
    while (millis() < pause + value)
    {
      yield(); //wait approx. [period] ms
    }
  }

  // Update current state of induction cooker
  void Update()
  {
    updatePower();

    isRelayon = updateRelay();

    if (isInduon && power > 0)
    {
      if (millis() > powerLast + powerSampletime)
      {
        powerLast = millis();
      }
      if (millis() > powerLast + powerHigh)
      {
        sendCommand(CMD[CMD_CUR - 1]);
        isPower = false;
      }
      else
      {
        sendCommand(CMD[CMD_CUR]);
        isPower = true;
      }
    }
    else if (isRelayon)
    {
      sendCommand(CMD[0]);
    }
  }

  // Update current state of the relay
  bool updateRelay()
  {
    // If power is requested, but relay is off: Turn relay on
    if (isInduon == true && isRelayon == false)
    {
      digitalWrite(PIN_WHITE, HIGH);
      return true;
    }

    // If power is not requested anymore, delay for fans has expired and relay is still on: Turn relay off
    if (isInduon == false && isRelayon == true)
    {
      if (millis() > timeTurnedoff + delayAfteroff)
      {
        digitalWrite(PIN_WHITE, LOW);
        return false;
      }
    }

    // If power is not requested and relay is already off: Keep relay off
    if (isInduon == false && isRelayon == false)
    {
      return false;
    }

    // If relay is on, keep it on
    return true;
  }

  // Update current state (power) of induction cooker
  void updatePower()
  {
    lastCommand = millis();

    if (power != newPower)
    { /* Neuer Befehl empfangen */

      if (newPower > 100)
      {
        newPower = 100; /* Nicht > 100 */
      }
      if (newPower < 0)
      {
        newPower = 0; /* Nicht < 0 */
      }
      power = newPower;

      timeTurnedoff = 0;
      isInduon = true;
      long difference = 0;

      if (power == 0)
      {
        CMD_CUR = 0;
        timeTurnedoff = millis();
        isInduon = false;
        difference = 0;
        goto setPowerLevel;
      }

      for (int i = 1; i < 7; i++)
      {
        if (power <= PWR_STEPS[i])
        {
          CMD_CUR = i;
          difference = PWR_STEPS[i] - power;
          goto setPowerLevel;
        }
      }

    setPowerLevel: /* Wie lange "HIGH" oder "LOW" */
      if (difference != 0)
      {
        powerLow = powerSampletime * difference / 20L;
        powerHigh = powerSampletime - powerLow;
      }
      else
      {
        powerHigh = powerSampletime;
        powerLow = 0;
      };
    }
  }

  // Update current state (power) of induction cooker
  /*
  void updatePower()
  {
    long difference = 0;
    lastCommand = millis();

    // New command with new power value received
    if (power != newPower)
    {
      if (newPower > 100){newPower = 100;}
      
      // Check if request is within possible range, otherwise turn off
      if (newPower <= 100 && newPower > 0)
      {
        power = newPower;
        isInduon = true;
        timeTurnedoff = 0;

        // Find out necessary power level for given power request and calculate difference
        for (int i = 1; i < 7; i++)
        {
          if (power <= PWR_STEPS[i])
          {
            CMD_CUR = i;
            difference = PWR_STEPS[i] - power;
          }
        }
      }
      else
      {
        // Simply turn off everything
        power = 0;
        CMD_CUR = 0;
        difference = 0;
        isInduon = false;
        timeTurnedoff = millis();
        // ToDo: Error handling
      }

      // If difference between requested power percentage and power level is given
      if (difference != 0)
      {
        powerLow = powerSampletime * difference / 20L;
        powerHigh = powerSampletime - powerLow;
      }
      else
      {
        powerHigh = powerSampletime;
        powerLow = 0;
      };
    }
  }*/

  // Send new command to induction cooker via serial connection
  void sendCommand(int command[33])
  {
    digitalWrite(PIN_YELLOW, HIGH);
    millis2wait(SIGNAL_START);
    digitalWrite(PIN_YELLOW, LOW);
    millis2wait(SIGNAL_WAIT);
    for (int i = 0; i < 33; i++)
    {
      digitalWrite(PIN_YELLOW, HIGH);
      delayMicroseconds(command[i]);
      digitalWrite(PIN_YELLOW, LOW);
      delayMicroseconds(SIGNAL_LOW);
    }
  }

  // Read information from induction cooker for error handling
  void readInput()
  {
    bool ishigh = digitalRead(PIN_INTERRUPT);
    unsigned long newInterrupt = micros();
    long signalTime = newInterrupt - lastInterrupt;

    // Filter jitter/flitch from serial line
    if (signalTime > 10)
    {
      if (ishigh)
      {
        lastInterrupt = newInterrupt; // PIN signal is rising, sending of bit has started
      }
      else
      { // Pin signal is Falling, bit transmission done - now evaluate results

        if (!inputStarted)
        { // search for starting bit
          if (signalTime < 35000L && signalTime > 15000L)
          {
            inputStarted = true;
            inputCurrent = 0;
          }
        }
        else
        { // Hat Begonnen. Nehme auf.
          if (inputCurrent < 34)
          { // nur bis 33 aufnehmen.
            if (signalTime < (SIGNAL_HIGH + SIGNAL_HIGH_TOL) && signalTime > (SIGNAL_HIGH - SIGNAL_HIGH_TOL))
            {
              // HIGH BIT erkannt
              inputBuffer[inputCurrent] = 1;
              inputCurrent += 1;
            }
            if (signalTime < (SIGNAL_LOW + SIGNAL_LOW_TOL) && signalTime > (SIGNAL_LOW - SIGNAL_LOW_TOL))
            {
              // LOW BIT erkannt
              inputBuffer[inputCurrent] = 0;
              inputCurrent += 1;
            }
          }
          else
          { // Aufnahme vorbei.

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
  unsigned long BtoI(int start, int numofbits)
  { //binary array to integer conversion
    unsigned long integer = 0;
    unsigned long mask = 1;
    for (int i = numofbits + start - 1; i >= start; i--)
    {
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
void readInputWrap()
{
  inductionCooker.readInput();
}

// Update induction cooker status, used for timer handling
void handleInduction()
{
  inductionCooker.Update();
}

// Convert sensor address to printable string
String addressToString(DeviceAddress deviceAddress)
{
  String address;
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16)
      address += "0";
    address += String(deviceAddress[i], HEX);
  }
  return address;
}

// Setup SSD1306 OLED I2C display
void setup_display()
{
  // Setup display #1 for sensor readings
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);

  // Setup display #2 for general information
  display2.init();
  display2.flipScreenVertically();
  display2.setFont(ArialMT_Plain_10);
}

// Setup One Wire temperature sensors
int setup_temp_sensors()
{
  // Start sensor configuration and count sensors
  sensorlist.begin();
  sensorlist.setResolution(SENSOR_RESOLUTION);
  numberOfDevices = sensorlist.getDeviceCount();
  display_writex(3, "Sensors: " + String(numberOfDevices), false);
  String output = "(";

  // Report parasite power requirements and identified sensors
  if (sensorlist.isParasitePowerMode())
  {
    output += "parasite power is: ON, ";
  }
  output = output + numberOfDevices + " sensors found(";

  // Loop through each device, print out address
  for (int i = 0; i < numberOfDevices; i++)
  {
    // Search the wire for address of healthy sensors
    if (sensorlist.getAddress(tempDeviceAddress, i))
    {
      output = output + i + "=" + addressToString(tempDeviceAddress) + ",";
      memcpy(sensorAdresses[i], tempDeviceAddress, sizeof(DeviceAddress));
    }
    // if not healthy report ghost sensors
    else
    {
      output = output + i + "=GHOST,";
      // ToDo: report error
    }
  }

  output += "))... ";
  syslog.log(LOG_INFO, output);
  Serial.print(output);
  return numberOfDevices;
}

// Setup GGM IDS2 induction cooker
void setup_induction()
{
  // Configure pins for serial connection
  pinMode(INDUCTION_PIN_WHITE, OUTPUT);
  digitalWrite(INDUCTION_PIN_WHITE, LOW);
  pinMode(INDUCTION_PIN_YELLOW, OUTPUT);
  digitalWrite(INDUCTION_PIN_YELLOW, HIGH);
  pinMode(INDUCTION_PIN_BLUE, INPUT_PULLUP); // Interrupt
  //attachInterrupt(digitalPinToInterrupt(INDUCTION_PIN_BLUE), readInputWrap, CHANGE);

  inductionCooker = induction();

  // Subscribe to MQTT topic for control
  String topic_string = String(MQTT_ROOT_PATH) + "/" + String(MQTT_DEVICE) + "/" + String(INDUCTION_MQTT_COMMANDS);
  char topic[50] = {};
  topic_string.toCharArray(topic, topic_string.length() + 1);
  mqttClient.subscribe(topic);
}

// Setup PID controller
void setup_pid()
{
  // Initialize the variables we're linked to
  PID_Input = 0;
  PID_Setpoint = 0;

  // Turn the PID on
  myPID.SetMode(AUTOMATIC);
  myPID.SetOutputLimits(Output_Limit_Min,Output_Limit_Max);

  // Subscribe to MQTT topic for control
  String topic_string = String(MQTT_ROOT_PATH) + "/" + String(MQTT_DEVICE) + "/" + String(PID_MQTT_TOPIC) + "/#";
  char topic[50] = {};
  topic_string.toCharArray(topic, topic_string.length() + 1);
  mqttClient.subscribe(topic);
}

// Callback whenever new MQTT message on a subscribed topic is received
void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  // Debugging
  String output = "MQTT: New message arrived at [" + String(topic) + "]: ";
  for (int i = 0; i < length; i++)
  {
    output += (char)payload[i];
  }
  Serial.println(output);

  // Interprete incoming JSON message
  StaticJsonDocument<256> doc;
  deserializeJson(doc, payload, length);

  // Induction control
  if (strcmp(topic, "cave/brewery/heater/power") == 0)
  {
    String state = doc["state"];
    int power = doc["power"];
    if (state == "off")
    {
      inductionCooker.newPower = 0;
      Serial.println("Induction: Changing power to 0");
    }
    else
    {
      inductionCooker.newPower = power;
      Serial.print("Induction: Changing power to ");
      Serial.println(power);
    }
    inductionCooker.Update(); // Trigger setting of new values at induction cooker
  }

  // PID: Turn on / off
  if (strcmp(topic, "cave/brewery/pid/control") == 0)
  {
    String state = doc["state"];
    if (state == "on")
    {
      PID_state = true;
      Serial.println("PID: Turning on PID controller!");
    }
    else
    {
      PID_state = false;
      Serial.println("PID: Turning off PID controller!");
    }
  }

  // PID: Reset Output
  if (strcmp(topic, "cave/brewery/pid/reset") == 0)
  {
    String output = doc["output"];
    if (output == "true")
    {
      Output = 0;
      Serial.println("PID: Reset of PID Output to 0 successfull!");
    }
    else
    {
      Serial.println("PID: Reset parameter wrong!");
    }
  }

  // PID: Change P proportion
  if (strcmp(topic, "cave/brewery/pid/p") == 0)
  {
    double P_prop = doc["P"];
    if (P_prop != PID_P)
    {
      Serial.print("PID: Changing P proportion from ");
      Serial.print(PID_P);
      Serial.print(" to ");
      Serial.println(P_prop);
      PID_P = P_prop;
    }
  }

  // PID: Change I proportion
  if (strcmp(topic, "cave/brewery/pid/i") == 0)
  {
    double I_prop = doc["I"];
    if (I_prop != PID_I)
    {
      Serial.print("PID: Changing I proportion from ");
      Serial.print(PID_I);
      Serial.print(" to ");
      Serial.println(I_prop);
      PID_I = I_prop;
    }
  }

  // PID: Change D proportion
  if (strcmp(topic, "cave/brewery/pid/d") == 0)
  {
    double D_prop = doc["D"];
    if (D_prop != PID_D)
    {
      Serial.print("PID: Changing D proportion from ");
      Serial.print(PID_D);
      Serial.print(" to ");
      Serial.println(D_prop);
      PID_D = D_prop;
    }
  }

  // PID: Set Target Temperature
  if (strcmp(topic, "cave/brewery/pid/target") == 0)
  {
    double targetTemperature = doc["targetTemperature"];
    PID_Setpoint = targetTemperature;
    Serial.println("PID: Setting target temperature for PID to " + String(targetTemperature));
  }
}

// General Setup Phase
void setup()
{
  // Serial configuration
  Serial.begin(SERIAL_BAUDRATE);
  Serial.print("Project version: ");
  Serial.println(VERSION);

  // Setup display
  setup_display();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.clear();

  // WIFI configuration
  Serial.print("Setting up Wifi connection... ");
  display_writex(0, "Wi-Fi:", false);
  WiFi.mode(WIFI_STA);
  WiFi.hostname(hostname);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.println("Wi-Fi connection failed! Rebooting...");
    display_writex(1, "Wi-Fi: Failed - Reboot", false);
    delay(5000);
    ESP.restart();
  }
  Serial.println("Done! (hostname=" + WiFi.hostname() + ", IP=" + WiFi.localIP().toString() + ")");
  display_writex(0, "Wi-Fi: " + WiFi.localIP().toString(), false);

  // mDNS configuration
  Serial.print("Setting up mDNS connection... ");
  display_writex(1, "MDNS:", false);
  if (!MDNS.begin(HOSTNAME)) {
    Serial.println("Error setting up MDNS responder!");
    display_writex(1, "MDNS: Failed", true);
  }
  Serial.println("Done!");
  display_writex(1, "MDNS: Started", false);

  // OTA configuration
  Serial.print("Setting up OTA connection... ");
  ArduinoOTA.setPort(OTA_UPDATE_PORT);
  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("OTA: Start updating " + type);
    display_writex(2, "OTA: Start updating...", true);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA End");
    display_writex(2, "OTA: Ended", true);
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
    display_writex(2, "OTA: Progress...", true);
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
  display_writex(2, "MQTT:", false);
  mqttClient.setServer(BROKER_ADDRESS, BROKER_PORT);
  mqttClient.connect(HOSTNAME, BROKER_USER, BROKER_PASSWORD);
  mqttClient.setCallback(mqttCallback);
  int counter = 0;
  while (!mqttClient.connected())
  {
    if (counter > 30)
    {
      syslog.log(LOG_INFO, "Startup not successfull, no connection to MQTT host could be established!");
      display_writex(2, "MQTT: Failed, Retrying", true);
    }
    delay(100);
    counter++;
  }
  Serial.println("Done!");
  display_writex(2, "MQTT: Started", false);

  // One Wire Bus configuration
  Serial.print("Setting up one wire sensors... ");
  display_writex(3, "Sensors: ", false);
  counter = 0;
  while((setup_temp_sensors() == 0) & (counter < 3)){
    Serial.println("Error: No sensors found!");
    counter++;
    delay(1000);
  }
  Serial.println("Done!");

  // Induction Cooker configuration
  Serial.print("Setting up induction cooker... ");
  display_writex(4, "Cooker: ", false);
  setup_induction();
  display_writex(4, "Cooker: Started", false);
  Serial.println("Done!");

  // PID configuration
  Serial.print("Setting up PID control... ");
  setup_pid();
  Serial.println("Done!");

  // Timer configuration
  timerTempRead.setInterval(FREQUENCY_READTEMP);
  timerTempRead.setCallback(temperature_read);
  if(PUBLISH_ENABLE){
    timerTempStatus.setInterval(FREQUENCY_STATUS);
    timerTempStatus.setCallback(publishStatus);
  }
  timerInductionStatus.setInterval(FREQUENCY_INDUCTION);
  timerInductionStatus.setCallback(handleInduction);
  timerDisplayUpdate.setInterval(FREQUENCY_DISPLAY);
  timerDisplayUpdate.setCallback(display_update);
  TimerManager::instance().start();

  syslog.log(LOG_INFO, "Startup successfull! (Version: " + String(VERSION) + ")");
  Serial.print("Setup: Successfull!\n");
  delay(2000);
}

// Read out temperature of all sensors
void temperature_read()
{
  float highest = 0;
  temperature_combined = 0;
  sensorlist.requestTemperatures();
  for (int i = 0; i < numberOfDevices; i++)
  {
    temperatures[i] = sensorlist.getTempCByIndex(i);
    temperature_combined += temperatures[i];
    // Find the highest sensor reading
    if (temperatures[i] > highest)
    {
      highest = temperatures[i];
    }
  }
  temperature_combined = temperature_combined / numberOfDevices;
  PID_Input = temperature_combined; // Forward highest reading to PID control
}

// Write current stored read out temperatures to serial output
void temperature_write_serial()
{
  String output = "Temperature(";
  if (numberOfDevices == 0)
  {
    output += "No sensors found!";
  }
  else
  {
    for (int i = 0; i < numberOfDevices; i++)
    {
      output = output + "Sensor" + i + ":" + temperatures[i] + "°C,";
    }
  }
  Serial.println(output + ")");
}

// Write current stored read out temperatures to MQTT topic
void temperature_write_mqtt()
{
  for (int i = 0; i < numberOfDevices; i++)
  {
    // Create JSON structure
    StaticJsonDocument<512> doc;
    //ToDo: doc["time"] = NOW();
    doc["temperature"] = temperatures[i];
    char message[512];
    serializeJson(doc, message);

    // Create topic character array and send information to MQTT topic
    String topic_string = String(MQTT_ROOT_PATH) + "/" + String(MQTT_DEVICE) + "/" + String(TEMPERATURE_MQTT_STATUS) + "/" + i;
    char topic[50] = {};
    topic_string.toCharArray(topic, topic_string.length() + 1);
    mqttClient.publish(topic, message);
  }
}

// Write current state of induction cooker to serial output
void induction_write_serial()
{
  String output = "Induction(isInduon=" + String(inductionCooker.isInduon) + ",isPower=" + inductionCooker.isPower + ",isRelayon=" + inductionCooker.isRelayon + ",power=" + inductionCooker.power + ",level=" + inductionCooker.CMD_CUR + ")";
  Serial.println(output);
}

// Write current state of induction cooker to MQTT topic
void induction_write_mqtt()
{
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
  String topic_string = String(MQTT_ROOT_PATH) + "/" + String(MQTT_DEVICE) + "/" + String(INDUCTION_MQTT_STATUS);
  char topic[50] = {};
  topic_string.toCharArray(topic, topic_string.length() + 1);

  // Publish information to MQTT topic
  mqttClient.publish(topic, message);
}

// Write current state of pid control to serial output
void pid_write_serial()
{
  String output = "PID(state=" + String(PID_state) + ",targetTemperature=" + PID_Setpoint + ",input=" + PID_Input + ",output=" + Output + ")";
  Serial.println(output);
}

// Write current state of pid control to MQTT topic
void pid_write_mqtt()
{
  // Create JSON structure
  StaticJsonDocument<256> doc;
  //ToDo: doc["time"] = NOW();
  doc["state"] = PID_state;
  doc["P"] = PID_P;
  doc["I"] = PID_I;
  doc["D"] = PID_D;
  doc["input"] = PID_Input;
  doc["targetTemperature"] = PID_Setpoint;
  doc["tempDiff"] = PID_Input - PID_Setpoint;
  doc["output"] = Output;
  char message[256];
  serializeJson(doc, message);

  // Create topic character array and send information to MQTT topic
  String topic_string = String(MQTT_ROOT_PATH) + "/" + String(MQTT_DEVICE) + "/" + String(PID_MQTT_TOPIC);
  char topic[100] = {};
  topic_string.toCharArray(topic, topic_string.length() + 1);
  mqttClient.publish(topic, message);
}

void display_update()
{
  // Erase old content
  display.clear();
  display2.clear();

  // Set layout of text, left aligned, Arial 10px
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display2.setTextAlignment(TEXT_ALIGN_LEFT);
  display2.setFont(ArialMT_Plain_10);

  // Control display
  display.drawString(0, 10, "WiFi = " + String(WiFi.status()) + ", MQTT = " + String(mqttClient.connected()));
  display.drawString(0, 20, "Sensors | " + String(temperatures[0]) + ", " + String(temperatures[1]));
  //display.drawString(0, 30, "Average | " + String((temperatures[0] + temperatures[1])/2));
  //display.drawString(0, 40, "Target    |  " + String(PID_Setpoint) + " | " + String((PID_Input - PID_Setpoint)));
  display.drawString(0, 50, "Power    |  " + String(inductionCooker.power) + "% - L" + String(inductionCooker.CMD_CUR));

  // System status display
  display2.drawString(0, 10, "WiFi = " + String(WiFi.status()) + ", MQTT = " + String(mqttClient.connected()));
  display2.drawString(0, 20, "Relay = " + String(inductionCooker.isRelayon) + ", Power = " + String(inductionCooker.isInduon));
  display2.drawString(0, 30, "PID = " + String(PID_state) + ", (" + String(PID_P) + "-" + String(PID_I) + "-" + String(PID_D) + ")");
  display2.drawString(0, 40, "PID output = " + String(Output));
  display2.drawString(0, 50, "");

  // Print buffer on display
  display.display();
  display2.display();
}

void display_writex(int row, const String& thisIsAString, bool reset)
{
  // Erase old content
  if(reset == true){
    display.clear();
  }
  

  // Set layout of text, left aligned, Arial 10px
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);

  // Control display
  display.drawString(0, row * 10, thisIsAString);

  // Print buffer on display
  display.display();
}

// Publish all states to outputs
void publishStatus()
{
  if(SERIAL_ENABLE){
    temperature_write_serial();
    induction_write_serial();
    pid_write_serial();
  }
  if(MQTT_ENABLE){
    temperature_write_mqtt();
    induction_write_mqtt();
    pid_write_mqtt();
  }
  
}

// Main loop
void loop()
{

  // Handle incoming OTA updates
  ArduinoOTA.handle();

  // Handle incoming MQTT mesages
  mqttClient.loop();
  if (!mqttClient.connected())
  {
    setup();
  }

  // Update timers (read temp, update status, etc.)
  TimerManager::instance().update();

  // PID control
  if (PID_state == true)
  {  
    myPID.SetTunings(PID_P, PID_I, PID_D);
    myPID.Compute();
    inductionCooker.newPower = Output;
    inductionCooker.Update();
  }
}
