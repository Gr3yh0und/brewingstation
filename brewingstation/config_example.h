// Serial
#define SERIAL_BAUDRATE 115200

// Network Configuration
#define HOSTNAME "ESP-BREWING"
#define SSID_NAME "SSID"
#define SSID_PASSWORD "PASSWORD"
#define SERVER_ADDRESS "192.168.0.100"
#define OTA_UPDATE_PORT 8266
#define MQTT_ROOT_PATH "root"
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

// SENSOR configuration DS18B20
#define SENSOR_NUMBER 2         // Amount of sensors
#define SENSOR_BUS_PIN 16       // Data bus pin
#define SENSOR_RESOLUTION 11    // SENSOR_RESOLUTION of DS18B20 sensors (9 - 12)
#define FREQUENCY_READTEMP 100  // Time how often temperature should be read
#define FREQUENCY_STATUS 600    // Time how often status updates are sent

// SENSOR configuration PT100X
#define SENSOR_PT100X_CS_PIN 22
#define SENSOR_PT100X_DI_PIN 4
#define SENSOR_PT100X_DO_PIN 19
#define SENSOR_PT100X_CLK_PIN 5
#define SENSOR_PT100X_R_REF 4300.0  // 430.0 for PT100
#define SENSOR_PT100X_R_NOM 1000.0  // 100.0 for PT100
#define SENSOR_PT100X_Config MAX31865_2WIRE

// SENSOR configuration BME280
#define SENSOR_BME280_OFFSET 3.2
#define SEALEVELPRESSURE_HPA (1018.00)  // ToDo: Get from API
#define SENSOR_BME280_CS_PIN 23
#define SENSOR_BME280_DI_PIN 4
#define SENSOR_BME280_DO_PIN 19
#define SENSOR_BME280_CLK_PIN 5

// I2C config
#define I2C_SDA_PIN 15
#define I2C_SCL_PIN 2

// DISPLAY configuration for both displays
#define DISPLAY_FREQUENCY 500
#define DISPLAY_SCREEN_WIDTH 128  // OLED display width, in pixels
#define DISPLAY_SCREEN_HEIGHT 64  // OLED display height, in pixels
#define DISPLAY_OLED_RESET -1     // Reset pin # (or -1 if sharing Arduino reset pin)

// Induction cooker configuration
#define FREQUENCY_INDUCTION 700    // Interval to update induction cooker power
#define INDUCTION_PIN_WHITE 32     // Relais
#define INDUCTION_PIN_YELLOW 33    // TX to cooker
#define INDUCTION_PIN_BLUE 21      // Interrupt
#define INDUCTION_FAN_DELAY 60000  // Default time for cooling fans after power off (factory default = 120000)
#define INDUCTION_MQTT_STATUS "heater"
#define INDUCTION_MQTT_COMMANDS "heater/power"
#define INDUCTION_LED_0_PIN 13
#define INDUCTION_LED_20_PIN 12
#define INDUCTION_LED_40_PIN 14
#define INDUCTION_LED_60_PIN 27
#define INDUCTION_LED_80_PIN 26
#define INDUCTION_LED_100_PIN 25
#define INDUCTION_LED_TEST_TIME 150