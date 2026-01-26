/*
* Snow Sensing Station - EnviroDIY Mayfly with LTE Bee
* Documented Version for Enhanced Readability
*/

// ==========================
// TinyGSM modem defines MUST be before includes
// ==========================
#define TINY_GSM_MODEM_SIM7080
#define TINY_GSM_RX_BUFFER 64
#define TINY_GSM_YIELD() { delay(2); }

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

// ===== MODEM CONFIGURATION =====
#define MODEM_SERIAL_BAUD_RATE 9600

// ===== HARDWARE PIN DEFINITIONS =====
const uint8_t SW_POWER_PIN = 22;
const uint8_t BEE_VCC_PIN = 18;
const uint8_t RELAY_COIL1_PIN = 10;
const uint8_t RELAY_COIL2_PIN = 11;
const int8_t modemVccPin = BEE_VCC_PIN;   // same as BEE_VCC_PIN
const int8_t modemStatusPin = 19;
const int8_t modemSleepRqPin = 23;
const uint8_t TEMP_SENSOR_PIN = 9;

// ===== THRESHOLDS / POLICY =====//  
const float ULTRASONIC_MIN_MM = 508;
const float ULTRASONIC_MAX_MM = 3048;
const int ALARM_THRESHOLD = 3;
const int MAX_TEXTS_PER_DAY = 3;
const int DAY_START_HOUR = 9;
const int DAY_END_HOUR = 17;

// Battery calc constants 
const uint8_t BATT_ADC_PIN = A6;
const float ADC_REF_V = 3.3;
const float ADC_COUNTS = 1024.0;
const float BATT_DIVIDER = 2.0;

// ===== GLOBALS & OBJECTS =====
RTC_DS3231 rtc;
Adafruit_ADS1115 ads;
SDI12 mySDI12(7);
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature tempSensor(&oneWire);

SerialWombatChip sw;
SerialWombatUART myUart(sw);

StreamDebugger debugger(Serial1, Serial);
TinyGsm modem(debugger);

const char apn[] = "hologram";

int pastHour = -1;
int currentHour = -1;
int apogeeFlag = 0;
String json;

bool testMode = true;
const unsigned long TEST_INTERVAL = 10000;

const char* SENSOR_NAMES[9] = {
  "ultrasonic", "Teros1", "Teros2", "Teros3", "Temp",
  "Pyr_Up", "Pyr_Dn", "Pyran_Up", "Pyran_Dn"
};

// ===== DATA STRUCTURES =====
struct SensorData {
DateTime timestamp;
float ultrasonic_distance_mm;
float pyrgeometer_up_mv;
float pyrgeometer_down_mv;
float pyranometer_up_mv;
float pyranometer_down_mv;
float air_temp_c;
String soil_moisture_teros, soil_moisture_teros2, soil_moisture_teros3;
float battery_voltage;
bool lte_connected;
bool sensor_errors[10];
};

SensorData data;

enum SensorType { ULTRASONIC, TEROS1, TEROS2, TEROS3, TEMP_SENSOR, PYRGEOMETER_UP, PYRGEOMETER_DOWN, PYRANOMETER_UP, PYRANOMETER_DOWN };
struct SensorStatus { SensorType type; int activateAlarm; int textSent; };
SensorStatus sensors[9];

// ================================================================
// UTILITY FUNCTIONS
// ================================================================

/* * Function: printSeparator
* ----------------------------
* Visual aid for Serial debugging. Separates sensor test sections.
* params: title (const char*) - The section name to display.
* returns: void
*/
void printSeparator(const char* title) {
Serial.println();
Serial.println(F("================================================"));  
Serial.println(title);
Serial.println(F("================================================")); 
}

/* * Function: printTestResult
* ----------------------------
* Standardized pass/fail output for sensor measurements.
* params: sensor (const char*); success (bool); value (float); unit (const char*).
* returns: void
*/
void printTestResult(const char* sensor, bool success, float value = 0, const char* unit = "") {
  Serial.print("[");
  Serial.print(success ? "PASS" : "FAIL");
  Serial.print("] ");
  Serial.print(sensor);
    if (success && unit[0] != '\0') {
      Serial.print(": ");
      Serial.print(value);
      Serial.print(" ");
      Serial.print(unit);
    }
  Serial.println();
}

/* * Function: getValue
* ----------------------------
* Extracts a specific numerical segment from a delimited SDI-12 response string.
* params: data (String) - The raw sensor string; separator (char); index (int).
* returns: String - The isolated value string.
*/
String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;

  for(int i=0; i<=maxIndex && found<=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
      found++;
      strIndex[0] = strIndex[1]+1;
      strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }
  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}

// ================================================================
// SYSTEM FUNCTIONS
// ================================================================

/* * Function: initializePowerSystem
* ----------------------------
* Boots Grove rails and sensor power via latching relays.
* params: none
* returns: void
*/
void initializePowerSystem() {
  printSeparator("POWER SYSTEM INITIALIZATION");

  pinMode(SW_POWER_PIN, OUTPUT);
  pinMode(BEE_VCC_PIN, OUTPUT);
  pinMode(RELAY_COIL1_PIN, OUTPUT);
  pinMode(RELAY_COIL2_PIN, OUTPUT);

  digitalWrite(SW_POWER_PIN, HIGH);
  Serial.println(F("Grove rails powered on")); 
  delay(2000);

  digitalWrite(RELAY_COIL1_PIN, HIGH);
  delay(50);
  digitalWrite(RELAY_COIL1_PIN, LOW);
  Serial.println(F("Main sensor power relay activated"));   
  delay(2000);

  digitalWrite(BEE_VCC_PIN, HIGH);
  Serial.println(F("LTE Bee powered on"));   
  delay(5000);
}

/* * Function: sendAlarm
* ----------------------------
* Dispatches an SMS alert via the GSM modem.
* params: message (String) - Core alert text.
* returns: void
*/
void sendAlarm(String message){
  modem.sendSMS("+13853218055", message + " needs to be attended to.");
}

/* * Function: WriteToSD
* ----------------------------
* Logs JSON data to the SD card with a MMDDHH.txt filename.
* params: json (String); data (SensorData&) - For timestamp access.
* returns: void
*/
void WriteToSD(String json, SensorData& data){
  if (!SD.begin(12)){
    Serial.println(F("ERROR: Failed to intialize SD card for writing."));   
    return;
  }

  String filename = "";
  if (data.timestamp.month() < 10) filename += "0";
  filename += String(data.timestamp.month());
  if (data.timestamp.day() < 10) filename += "0";
  filename += String(data.timestamp.day());
  if (data.timestamp.hour() < 10) filename += "0";
  filename += String(data.timestamp.hour());
  filename += ".txt";

  File dataFile = SD.open(filename, FILE_WRITE);
  if (dataFile) {
    dataFile.println(json);
    dataFile.close();
    Serial.print(F("Successfully wrote new file: "));   
    Serial.println(filename);
  } else {
    Serial.print(F("ERROR: Could not create file "));   
    Serial.print(filename);
    Serial.println(F(" for writing."));   
  }
}

// ================================================================
// SENSOR TESTS
// ================================================================

/* * Function: testMaxBotixSensor
* ----------------------------
* Interfaces with MaxBotix ultrasonic rangefinder via SerialWombat UART.
* returns: float - Distance in mm; -1 on failure.
*/
float testMaxBotixSensor() {
  printSeparator("MAXBOTIX ULTRASONIC TEST");

  myUart.begin(9600,7,9,7,1);
  delay(100);

  Serial.println(F("Requesting ultrasonic measurement..."));   
  myUart.print("R");

  unsigned long start_time = millis();
  String response = "";

  while (millis() - start_time < 2000) {
    if (myUart.available()) {
      char c = myUart.read();
      if (c == 'R') response = "";
      else if (c >= '0' && c <= '9') response += c;
      else if (c == '\r' || c == '\n') break;
    }
  }

  myUart.disable();

  if (response.length() > 0) {
    float distance = response.toFloat();
    Serial.print(F("Ultrasonic distance: "));   
    Serial.print(distance);
    Serial.println(F("mm"));   

    bool outOfRange = (distance > ULTRASONIC_MAX_MM || distance < ULTRASONIC_MIN_MM);
    if (outOfRange && sensors[0].textSent < MAX_TEXTS_PER_DAY) {
      sensors[0].activateAlarm += 1;
    }
    return distance;

  } else {
  Serial.println(F("ERROR: No response from MaxBotix sensor"));   
  sensors[0].activateAlarm +=1;
  return -1;
  }
}

/* * Function: testApogeeRadiation
* ----------------------------
* Reads Pyranometer/Pyrgeometer mV levels via ADS1115 ADC.
* params: data (SensorData&) - Struct to store raw results.
* returns: bool - Success status.
*/
bool testApogeeRadiation(SensorData& data) {
  printSeparator("APOGEE RADIATION SENSORS TEST");
  Serial.println(F("Reading ADS1115 channels..."));   

  DateTime now = rtc.now();
  bool daytime = (now.hour() > DAY_START_HOUR && now.hour() < DAY_END_HOUR);

  data.pyrgeometer_up_mv = ads.readADC_SingleEnded(0) * ads.computeVolts(1) * 1000;
  delay(50);
  if (daytime && data.pyrgeometer_up_mv == 0) sensors[5].activateAlarm += 1;

  data.pyrgeometer_down_mv = ads.readADC_SingleEnded(1) * ads.computeVolts(1) * 1000;
  delay(50);
  if (daytime && data.pyrgeometer_down_mv == 0) sensors[6].activateAlarm += 1;

  data.pyranometer_up_mv = ads.readADC_SingleEnded(2) * ads.computeVolts(1) * 1000;
  delay(50);
  if (daytime && data.pyranometer_up_mv == 0) sensors[7].activateAlarm += 1;

  data.pyranometer_down_mv = ads.readADC_SingleEnded(3) * ads.computeVolts(1) * 1000;
  if (daytime && data.pyranometer_down_mv == 0) sensors[8].activateAlarm += 1;

  else sensors[8].activateAlarm = 0;

  Serial.print(F("Pyrgeometer Up: "));
  Serial.print(data.pyrgeometer_up_mv, 2);
  Serial.println(F(" mV"));

  Serial.print(F("Pyrgeometer Down: "));
  Serial.print(data.pyrgeometer_down_mv, 2);
  Serial.println(F(" mV"));

  Serial.print(F("Pyranometer Up: "));
  Serial.print(data.pyranometer_up_mv, 2);
  Serial.println(F(" mV"));

  Serial.print(F("Pyranometer Down: "));
  Serial.print(data.pyranometer_down_mv, 2);
  Serial.println(F(" mV"));


  return true;
}

/* * Function: testTemperatureSensor
* ----------------------------
* Requests reading from DS18B20 digital temperature probe.
* returns: float - Temp in Celsius; -999 on error.
*/
float testTemperatureSensor() {
  printSeparator("TEMPERATURE SENSOR TEST");
  Serial.println(F("Reading DS18B20 temperature sensor..."));

  tempSensor.requestTemperatures();
  delay(750);

  float temp = tempSensor.getTempCByIndex(0);
  if(temp > 49) sendAlarm("Temp unrealistically high.");
  if(temp < -20.55) sendAlarm("Temp unrealistically low.");

  if (temp != DEVICE_DISCONNECTED_C) {
    Serial.print(F("Temperature: ")); 
    Serial.print(temp, 2); 
    Serial.println(F("°C"));
    return temp;
  } else {
    Serial.println(F("ERROR: Temp sensor failed"));
    sensors[4].activateAlarm +=1;
    return -999;
  }
}

/* * Function: testSoilMoisture
* ----------------------------
* Interfaces with Teros-12 sensors on addresses 1, c, and b via SDI-12 protocol.
* params: data (SensorData&) - Struct for moisture strings.
* returns: bool - Data captured successfully.
*/
bool testSoilMoisture(SensorData& data) {
  printSeparator("SOIL MOISTURE SENSORS TEST");

  const char* addrs[] = {"1", "c", "b"};
  String* targets[] = {&data.soil_moisture_teros, &data.soil_moisture_teros2, &data.soil_moisture_teros3};

  for(int i=0; i<3; i++){
    Serial.println("Reading Teros Address " + String(addrs[i]));

    mySDI12.sendCommand(String(addrs[i]) + "M!");
    delay(2000);

    mySDI12.clearBuffer();
    mySDI12.sendCommand(String(addrs[i]) + "D0!");
    delay(30);

    String resp = "";
    while (mySDI12.available()) {
      char c = mySDI12.read();
      if (c != '\n' && c != '\r') resp += c;
      }

    if (resp.length() == 0){
      Serial.println("No measurement from " + String(addrs[i]));
      sensors[i+1].activateAlarm +=1;
    } else {
      *targets[i] = resp;
      Serial.println("Soil Moisture is " + resp);
    }
  }
  return true;
}

/* * Function: createDataJSON
* ----------------------------
* Formats collected sensor readings into a standard JSON string.
* params: data (SensorData&) - Source measurements.
* returns: String - Formatted JSON text.
*/
String createDataJSON(const SensorData& data) {
  String out = "{\n";
  out += " \"timestamp\": \"" + data.timestamp.timestamp() + "\",\n";
  out += " \"radiation\": {\n";
  out += " \"pyrgeo_up\": " + String(data.pyrgeometer_up_mv, 2) + ",\n";
  out += " \"pyrgeo_dn\": " + String(data.pyrgeometer_down_mv, 2) + ",\n";
  out += " \"pyrano_up\": " + String(data.pyranometer_up_mv, 2) + ",\n";
  out += " \"pyrano_dn\": " + String(data.pyranometer_down_mv, 2) + "\n";
  out += " },\n";
  out += " \"environment\": {\n";
  out += " \"air_temp_c\": " + String(data.air_temp_c, 2) + "\n";
  out += " },\n";
  out += " \"soil\": {\n";
  out += " \"s1\": " + data.soil_moisture_teros + ",\n";
  out += " \"s2\": " + data.soil_moisture_teros2 + ",\n";
  out += " \"s3\": " + data.soil_moisture_teros3 + "\n";
  out += " },\n";
  out += " \"system\": {\n";
  out += " \"batt\": " + String(data.battery_voltage, 2) + "\n";
  out += " }\n}";
  return out;
}

// ================================================================
// CORE LOOP
// ================================================================

/* * Function: setup
* ----------------------------
* Initializes all hardware, library protocols, and global variables once.
* returns: void
*/
void setup() {
  Serial.begin(115200);
  while (!Serial) delay(100);

  // Define Sensor List
  sensors[0].type = ULTRASONIC; sensors[1].type = TEROS1; sensors[2].type = TEROS2;
  sensors[3].type = TEROS3; sensors[4].type = TEMP_SENSOR; sensors[5].type = PYRGEOMETER_UP;
  sensors[6].type = PYRGEOMETER_DOWN; sensors[7].type = PYRANOMETER_UP; sensors[8].type = PYRANOMETER_DOWN;

  initializePowerSystem();

  Wire.begin();
  sw.begin(0x6B);

  if (!rtc.begin()) Serial.println(F("ERROR: RTC missing!"));
  if (!ads.begin()) apogeeFlag = 1;

  tempSensor.begin();
  mySDI12.begin();

  // Modem Sequence
  pinMode(modemVccPin, OUTPUT); 
  pinMode(modemStatusPin, INPUT); 
  pinMode(modemSleepRqPin, OUTPUT);

  digitalWrite(modemVccPin, HIGH); 
  digitalWrite(modemSleepRqPin, LOW);

  Serial1.begin(MODEM_SERIAL_BAUD_RATE);
  
  if (modem.init()) {
    modem.gprsConnect(apn);
    modem.sendAT("+CSMP=31,167,0,0");
  }
}

/* * Function: loop
* ----------------------------
* Repeatedly processes SMS alerts, executes test cycles, and logs hourly data.
* returns: void
*/
void loop() {
    DateTime now = rtc.now();
    currentHour = now.hour();

    // Reset counters at Midnight
    if (currentHour == 0) {
      for (int i = 0; i < 9; i++) {
        sensors[i].activateAlarm = 0; 
        sensors[i].textSent = 0;
        } 
    }

  // Process Alarms
  for(int i=0; i<9; i++){
    bool alarmTriggered = (sensors[i].activateAlarm >= ALARM_THRESHOLD);
    bool textsRemaining = (sensors[i].textSent < MAX_TEXTS_PER_DAY);
    bool shouldText = alarmTriggered && textsRemaining;

    if (shouldText) {
      sendAlarm(SENSOR_NAMES[i]);
      sensors[i].textSent++;
    }
  }

  if (testMode) {
    printSeparator("STARTING CYCLE");

    data.ultrasonic_distance_mm = testMaxBotixSensor();
    testApogeeRadiation(data);
    data.air_temp_c = testTemperatureSensor();
    testSoilMoisture(data);

    // Battery & Time 
    data.battery_voltage = (analogRead(BATT_ADC_PIN) * ADC_REF_V / ADC_COUNTS) * BATT_DIVIDER;
    data.timestamp = rtc.now();

    json = createDataJSON(data);
    Serial.println(json);

    // Hourly Write
    if (currentHour != pastHour){
      for(int i = 0; i < 9; i ++){
        sensors[i].activateAlarm = 0;
      } 
      WriteToSD(json, data);
    }

    delay(TEST_INTERVAL);
    pastHour = currentHour;
  }
}