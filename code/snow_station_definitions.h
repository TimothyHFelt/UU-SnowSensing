#ifndef SNOW_STATION_DEFINITIONS_H
#define SNOW_STATION_DEFINITIONS_H
#include <Arduino.h>
#include <SDI12.h>
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <RTClib.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_ADS1X15.h>
#include <TinyGsmClient.h>
#include <StreamDebugger.h>
#include <SerialWombat.h>
#include <SerialWombatUART.h>

// Set serial baud rates
#define MODEM_SERIAL_BAUD_RATE 9600

// Include modem library
#define TINY_GSM_MODEM_SIM7080
#define TINY_GSM_RX_BUFFER 64
#define TINY_GSM_YIELD() { delay(2); }

// ===== HARDWARE PIN DEFINITIONS =====
// Power control
extern volatile bool hourly_write_pending;
const uint8_t SW_POWER_PIN = 22;    // Grove rail power
const uint8_t BEE_CTS_PIN = -1;
const uint8_t BEE_VCC_PIN = 18;     // Bee socket power (for LTE Bee)
const uint8_t RELAY_COIL1_PIN = 10; // Latching relay coil 1 (power on)
const uint8_t RELAY_COIL2_PIN = 11; // Latching relay coil 2 (power off)
const int8_t modemVccPin     = 18;
const int8_t modemStatusPin  = 19;
const int8_t modemResetPin   = -1;
const int8_t modemSleepRqPin = 23;
const String errorLog = "ErrorLog.txt";
const String batteryLog = "BatteryLog.txt";
extern int apogeeFlag;
extern SerialWombatChip sw;
extern SerialWombatUART myUart;
extern String myBuffer;
extern String json;
extern bool sd_ok;
extern int readingCount;

extern StreamDebugger debugger;
extern TinyGsm modem;
extern int pastHour;
extern int currentHour;
extern int textCount;

// LiDAR (UART1 on J3 header) - gets power from J3 5V
//#define LIDAR_SERIAL Serial1 
const uint32_t LIDAR_BAUD = 115200;

// Soil moisture analog sensors (J3 header)
const uint8_t SOIL_ANALOG_1_PIN = A1;
const uint8_t SOIL_ANALOG_2_PIN = A2;

// Voltage Reader
const uint8_t VOLTAGE_READER = A3;
// Teros-12 SDI-12 (D4-7 Grove)
extern SDI12 mySDI12;

// Temperature sensor (if using OneWire DS18B20)
const uint8_t TEMP_SENSOR_PIN = 9; // Adjust based on actual connection
extern OneWire oneWire;
extern DallasTemperature tempSensor;

//pyrgeometer Constants
const float K1 = 9.033;
const float K2 = 1.024;
const float ST = .0000000567;

//pyranometer Constants
const float CFU = 17.5;
const float CFD = 6.7;

const char apn[] = "hologram";

// ===== SENSOR OBJECTS =====
extern RTC_DS3231 rtc;
extern Adafruit_ADS1115 ads;  // ADS1115 for Apogee sensors
extern Adafruit_ADS1115 ads1;
extern Adafruit_ADS1115 ads2;

// ===== SYSTEM CONTROL =====
extern bool testMode;  // Set to false for production 15-minute intervals
const unsigned long TEST_INTERVAL = 10000; // 10 seconds between tests

// ===== DATA STRUCTURE =====
struct SensorData {
  // Timestamp
  DateTime timestamp;
  
  // Snow depth measurements
  //float lidar_distance_mm;
  float ultrasonic_distance_mm;
  
  // Radiation measurements (mV from Apogee sensors)
  float pyrgeometer_up_mv;
  float pyrgeometer_down_mv;
  float pyranometer_up_mv;
  float pyranometer_down_mv;
  
  // Temperature
  float air_temp_c;
  
  // Soil measurements
  String soil_moisture_teros;
  String soil_moisture_teros2;
  String soil_moisture_teros3;

  String systemAlarm = "";
  String LowVoltageAlarm = "";
  
  // System status
  float battery_voltage;
  bool lte_connected;
  
  // Error flags
  bool sensor_errors[10];
};

extern SensorData data;

/*
 * SENSOR INDEX MAPPING (sensors[] array):
 * [0] ULTRASONIC        [5] PYRGEOMETER_UP
 * [1] TEROS1            [6] PYRGEOMETER_DOWN
 * [2] TEROS2            [7] PYRANOMETER_UP
 * [3] TEROS3            [8] PYRANOMETER_DOWN
 * [4] TEMP_SENSOR
 */
enum SensorType {
  ULTRASONIC,
  TEROS1,
  TEROS2,
  TEROS3,
  TEMP_SENSOR,
  PYRGEOMETER_UP, 
  PYRGEOMETER_DOWN,
  PYRANOMETER_UP,
  PYRANOMETER_DOWN,
  BATTERY_VOLTAGE,
};

struct SensorStatus {
  SensorType type;
  int activateAlarm;    // Use a boolean to indicate if the alarm is currently sounding/active
};




extern SensorStatus sensors[10];




#endif
