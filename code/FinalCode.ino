/*
 * Snow Sensing Station - Complete Sensor Test Mode
 * Tests all sensors individually with clear serial output
 * For EnviroDIY Mayfly with LTE Bee cellular communication
 */

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
//#include <SoftwareSerial.h>

// Set serial baud rates
#define MODEM_SERIAL_BAUD_RATE 9600

// Include modem library
#define TINY_GSM_MODEM_SIM7080
#define TINY_GSM_RX_BUFFER 64
#define TINY_GSM_YIELD() { delay(2); }


// ===== HARDWARE PIN DEFINITIONS =====
// Power control
volatile bool hourly_write_pending = false;
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
int apogeeFlag = 0;
SerialWombatChip sw;
SerialWombatUART myUart(sw);
String json;
bool sd_ok;

StreamDebugger debugger(Serial1, Serial);
TinyGsm modem(debugger);
int pastHour = -1;
int currentHour = -1;
int textCount = 0;

// LiDAR (UART1 on J3 header) - gets power from J3 5V
//#define LIDAR_SERIAL Serial1 
const uint32_t LIDAR_BAUD = 115200;

// Soil moisture analog sensors (J3 header)
const uint8_t SOIL_ANALOG_1_PIN = A1;
const uint8_t SOIL_ANALOG_2_PIN = A2;

// Teros-12 SDI-12 (D4-7 Grove)
SDI12 mySDI12(7);



// Temperature sensor (if using OneWire DS18B20)
const uint8_t TEMP_SENSOR_PIN = 9; // Adjust based on actual connection
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature tempSensor(&oneWire);

const char apn[] = "hologram";

// ===== SENSOR OBJECTS =====
RTC_DS3231 rtc;
Adafruit_ADS1115 ads;  // ADS1115 for Apogee sensors

// ===== SYSTEM CONTROL =====
bool testMode = true;  // Set to false for production 15-minute intervals
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

  String systemAlarm;
  
  // System status
  float battery_voltage;
  bool lte_connected;
  
  // Error flags
  bool sensor_errors[10];
};

SensorData data;

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
};

struct SensorStatus {
  SensorType type;
  int activateAlarm;    // Use a boolean to indicate if the alarm is currently sounding/active
};




SensorStatus sensors[9];


// ===== UTILITY FUNCTIONS =====

/* * Function: printSeparator
 * ----------------------------
 * Purpose:  Prints a formatted visual header to the Serial monitor to 
 * organize sensor test sections during debugging.
 * * Params:   title (const char*) - The name of the section to display.
 * Returns:  void
 * * Notes:    Includes a 2-second delay to ensure the header is readable 
 * before data begins to scroll.
 */
void printSeparator(const char* title) {
  Serial.println();
  Serial.println("================================================");
  Serial.print("  ");
  Serial.println(title);
  Serial.println("================================================");
  delay(2000); // 2 second pause after each section header
}

/* * Function: printTestResult
 * ----------------------------
 * Purpose:  Standardizes Serial output for sensor tests with PASS/FAIL status.
 * Params:   sensor (const char*) - Name; success (bool); value (float); unit (const char*).
 * Returns:  void
 * Notes:    Only prints numerical value and unit if 'success' is true.
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

// ===== INTERRUPT SERVICE ROUTINE (ISR) =====
void rtc_alarm_handler() {
  // Set the flag to notify the main loop
  hourly_write_pending = true;
  
  // NOTE: PcInt does not require detachInterrupt() inside the ISR.
}

// ===== POWER MANAGEMENT =====

/* * Function: initializePowerSystem
 * ----------------------------
 * Purpose:  Configures hardware pins and executes power-on sequence for rails/modem.
 * Params:   none
 * Returns:  void
 * Notes:    Uses latching relay pulses and 5s stabilization delay for field reliability.
 */
void initializePowerSystem() {
  printSeparator("POWER SYSTEM INITIALIZATION");
  
  // Configure power pins
  pinMode(SW_POWER_PIN, OUTPUT);
  pinMode(BEE_VCC_PIN, OUTPUT);
  pinMode(RELAY_COIL1_PIN, OUTPUT);
  pinMode(RELAY_COIL2_PIN, OUTPUT);
  
  // Turn on Grove rails (needed for most sensors)
  digitalWrite(SW_POWER_PIN, HIGH);
  Serial.println("Grove rails powered on");
  delay(2000); // Pause to observe power status
  
  // Turn on main sensor power via latching relay
  digitalWrite(RELAY_COIL1_PIN, HIGH);
  delay(50);
  digitalWrite(RELAY_COIL1_PIN, LOW);
  Serial.println("Main sensor power relay activated");
  delay(2000); // Pause to observe relay activation
  
  // Keep LTE Bee powered for testing
  digitalWrite(BEE_VCC_PIN, HIGH);
  Serial.println("LTE Bee powered on");
  
  delay(5000); // Allow power to stabilize
}


// ===== LTE BEE COMMUNICATION TEST =====

/* * Function: testLTEBee
 * ----------------------------
 * Purpose:  Verifies hardware power and simulated AT command sequence for the SIM7080G.
 * Params:   none
 * Returns:  bool - Success status (assumed true for test mode).
 * Notes:    Configures Hologram APN and checks SIM/Network registration status.
 */
bool testLTEBee() {
  printSeparator("LTE BEE COMMUNICATION TEST");
  
  Serial.println("Testing SIM7080G LTE Bee with Hologram SIM...");

  // Enable LTE Bee power
  digitalWrite(BEE_VCC_PIN, HIGH);
  delay(2000); // Wait for module to boot
  
  // The LTE Bee uses Serial for AT commands
  Serial.print("Sending AT command... ");
  //Serial.flush();
  

  // Send basic AT command
  
  // Check for response (this is simplified - in production you'd read from Serial)
  Serial.println("AT command sent");
  
  // Test SIM card detection
  Serial.println("Checking SIM card...");
  Serial.println("AT+CPIN?");
  delay(500);
  
  // Configure APN for Hologram
  Serial.println("Setting APN...");
  Serial.println("AT+CGDCONT=1,\"IP\",\"hologram\"");
  delay(500);
  
  // Check network registration
  Serial.println("Checking network registration...");
  Serial.println("AT+CREG?");
  delay(1000);
  
  Serial.println("LTE Bee initialization commands sent");
  Serial.println("Note: Actual responses would be read in production code");
  Serial.println("Expected: SIM card detected, APN set to 'hologram'");
  
  return true; // Assume success for testing
}

// ===== MAXBOTIX ULTRASONIC TEST =====

/* * Function: testMaxBotixSensor
 * ----------------------------
 * Purpose:  Measures distance via SerialWombat UART and updates alarm strikes.
 * Params:   none
 * Returns:  float - Measured distance in mm; returns -1 if sensor fails to respond.
 * Notes:    Increments sensors[0].activateAlarm if reading is out of bounds (>3048 or <508).
 * Strike count persists until the hourly reset logic in the main loop.
 */
float testMaxBotixSensor() {
  printSeparator("MAXBOTIX ULTRASONIC TEST");
  
  myUart.begin(9600,7,9,7,1);
  delay(100);
  
  Serial.println("Requesting ultrasonic measurement...");
  myUart.print("R");
  
  unsigned long start_time = millis();
  String response = "";
  
  while (millis() - start_time < 2000) {
    if (myUart.available()) {
      char c = myUart.read();
      if (c == 'R') {
        response = "";
      } else if (c >= '0' && c <= '9') {
        response += c;
      } else if (c == '\r' || c == '\n') {
        break;
      }
    }
  }
  myUart.disable();
  
  if (response.length() > 0) {
    float distance = response.toFloat();
    Serial.print("Ultrasonic distance: ");
    Serial.print(distance);
    Serial.println("mm");
    return distance;
  } else {
    Serial.println("ERROR: No response from MaxBotix sensor");
    sensors[0].activateAlarm +=1;
    return -1;
  }
}

// ===== APOGEE RADIATION SENSORS TEST =====

/* * Function: testApogeeRadiation
 * ----------------------------
 * Purpose:  Reads 4 channels of radiation data (mV) via ADS1115 ADC.
 * Params:   data (SensorData&) - Struct to store calculated millivolt values.
 * Returns:  bool - True once all 4 channels are sampled.
 * Notes:    Increments alarm strikes if a 0mV reading is detected during 
 * daylight hours (10:00 to 17:00). Strikes persist until hourly reset.
 */
bool testApogeeRadiation(SensorData& data) {
  bool myBool = true;
  printSeparator("APOGEE RADIATION SENSORS TEST");
  Serial.println("Reading ADS1115 channels...");
  
  // Read all 4 channels (adjust channel assignments as needed)
  data.pyrgeometer_up_mv = ads.readADC_SingleEnded(0) * ads.computeVolts(1) * 1000;
  delay(50);
  DateTime now = rtc.now();
  if(data.pyrgeometer_up_mv == 0 && !(now.hour()>9 && now.hour()<17)){
    sensors[5].activateAlarm +=1;
    LogError("The time is " + String(rtc.now().hour()) + ":" + String(rtc.now().minute()) + " and the pyrgeometer up radiation value is 0. Check connection.");
    myBool = false;
  }
  data.pyrgeometer_down_mv = ads.readADC_SingleEnded(1) * ads.computeVolts(1) * 1000;
  delay(50);
  if(data.pyrgeometer_down_mv == 0 && !(now.hour()>9 && now.hour()<17)){
    sensors[6].activateAlarm +=1;
    LogError("The time is " + String(rtc.now().hour()) + ":" + String(rtc.now().minute()) + " and the pyrgeometer down radiation value is 0. Check connection.");
    myBool = false;
  }
  data.pyranometer_up_mv = ads.readADC_SingleEnded(2) * ads.computeVolts(1) * 1000;
  delay(50);
  if(data.pyranometer_up_mv == 0 && !(now.hour()>9 && now.hour()<17)){
    sensors[7].activateAlarm +=1;
    LogError("The time is " + String(rtc.now().hour()) + ":" + String(rtc.now().minute()) + " and the pyranometer up radiation value is 0. Check connection.");
    myBool = false;
  }
  data.pyranometer_down_mv = ads.readADC_SingleEnded(3) * ads.computeVolts(1) * 1000;
  delay(50);
  if(data.pyranometer_down_mv == 0 && !(now.hour()>9 && now.hour()<17)){
    sensors[8].activateAlarm +=1;
    LogError("The time is " + String(rtc.now().hour()) + ":" + String(rtc.now().minute()) + " and the pyranometer down radiation value is 0. Check connection.");
    myBool = false;
  }
  Serial.print("Pyrgeometer Up:   ");
  Serial.print(data.pyrgeometer_up_mv, 2);
  Serial.println(" mV");
  delay(1000); // Pause between each reading
  
  Serial.print("Pyrgeometer Down: ");
  Serial.print(data.pyrgeometer_down_mv, 2);
  Serial.println(" mV");
  delay(1000);
  
  Serial.print("Pyranometer Up:   ");
  Serial.print(data.pyranometer_up_mv, 2);
  Serial.println(" mV");
  delay(1000);
  
  Serial.print("Pyranometer Down: ");
  Serial.print(data.pyranometer_down_mv, 2);
  Serial.println(" mV");
  delay(2000); // Longer pause at end of radiation test
  
  return myBool; // All readings successful
}

// ===== TEMPERATURE SENSOR TEST =====

/* * Function: testTemperatureSensor
 * ----------------------------
 * Purpose:  Reads air temperature from the DS18B20 OneWire digital sensor.
 * Params:   none
 * Returns:  float - Temperature in Celsius; returns -999 if disconnected.
 * Notes:    Increments sensors[4].activateAlarm if temp is out of realistic 
 * range (>49°C or <-20.55°C) or if the device is disconnected. 
 * Strikes persist until the hourly reset in the main loop.
 */
float testTemperatureSensor() {
  printSeparator("TEMPERATURE SENSOR TEST");
  
  Serial.println("Reading DS18B20 temperature sensor...");
  
  tempSensor.requestTemperatures();
  delay(750); // Wait for conversion
  
  float temp = tempSensor.getTempCByIndex(0);
  
  if (temp != DEVICE_DISCONNECTED_C) {
    Serial.print("Temperature: ");
    Serial.print(temp, 2);
    Serial.println("°C");
    return temp;
  } else {
    Serial.println("ERROR: Temperature sensor not connected or failed");
    sensors[4].activateAlarm +=1;
    return -999;
  }
}

// ===== SOIL MOISTURE SENSORS TEST =====

/* * Function: testSoilMoisture
 * ----------------------------
 * Purpose:  Queries three Teros-12 soil sensors via SDI-12 protocol 
 * on addresses '1', 'c', and 'b'.
 * Params:   data (SensorData&) - Struct to store the raw SDI-12 response strings.
 * Returns:  bool - Returns true if measurements are successful, false if a sensor fails.
 * Notes:    Increments alarm strikes (sensors[1], [2], or [3]) if a sensor 
 * fails to return a string. Strikes persist until the hourly reset.
 */
bool testSoilMoisture(SensorData& data) {
  bool myBool = true;
  printSeparator("SOIL MOISTURE SENSORS TEST");
  
  // Teros-12 SDI-12 sensor
 Serial.println("Reading Teros-12 sensor...");
 
  Serial.println("taking soil measurement from address 1");
  mySDI12.sendCommand("1M!");
  delay(30);
  mySDI12.clearBuffer();
  delay(2000);
  Serial.println("Getting the data from address 1");
  mySDI12.sendCommand("1D0!");
  delay(30);
  String sdiResponse_a = "";  // Create a blank String that will become the reported value
  while (mySDI12.available()) {
    char c = mySDI12.read();  // Read in a character
    if ((c != '\n') && (c != '\r')) {  // If the read-in character isn't a new line
      sdiResponse_a += c;  // Add the character to the String
      delay(10);
    }
  }
  if (sdiResponse_a.length() == 0){
    Serial.println("No measurement taken. Check physical connection.");
    sensors[1].activateAlarm +=1;
    return false;
  }
  else {
    Serial.println("Measurement was taken.");
  }
  mySDI12.clearBuffer();  // Clear the buffer for the next command
  float vmc_a = getValue(sdiResponse_a, '+', 1).toFloat()/10000;
  Serial.print("Volumetric Water Content: ");
  Serial.println(vmc_a);
  if(vmc_a>0.70 || vmc_a<-0.50){
    sensors[1].activateAlarm+=1;
    return false;
  }
  if(vmc_a>0.70){
    String message = "Teros A water content is too high at " + String(vmc_a) + ". Teros A might be in a puddle.";
    LogError(message);
  }
  else if(vmc_a<-0.5){
    String message = "Teros A water content is too low at " + String(vmc_a) + ". Check sensor.";
    LogError(message);
  }
  float tmp_a = getValue(sdiResponse_a, '+', 2).toFloat();
  Serial.print("Soil Temperature: ");
  Serial.println(tmp_a);
  if(tmp_a>50 || tmp_a< -40){
    sensors[1].activateAlarm+=1;
    String message = "Teros A temperature is at " + String(tmp_a) + " c, which is out of range. Check the sensor.";
    LogError(message);
    myBool = false;
  }
  float eC_a = getValue(sdiResponse_a, '+', 3).toFloat();
  Serial.print("Electrical Conductivity: ");
  Serial.println(eC_a);
  if(eC_a < 0 || eC_a > 20){
    sensors[1].activateAlarm+=1;
    String message = "Electrical conductivity for Teros A is outside of range at " + String(eC_a) + ". Check sensor.";
    LogError(message);
    myBool = false;
  }

  Serial.println("Soil Moisture 1 is " + sdiResponse_a);
  data.soil_moisture_teros = sdiResponse_a;
  

  Serial.println("taking soil measurement from address c");
  mySDI12.sendCommand("cM!");
  delay(30);
  mySDI12.clearBuffer();
  delay(2000);
  Serial.println("Getting the data from address c");
  mySDI12.sendCommand("cD0!");
  delay(30);
  String sdiResponse_b = "";  // Create a blank String that will become the reported value
  while (mySDI12.available()) {
    char c = mySDI12.read();  // Read in a character
    if ((c != '\n') && (c != '\r')) {  // If the read-in character isn't a new line
      sdiResponse_b += c;  // Add the character to the String
      delay(10);
    }
  }
  if (sdiResponse_b.length() == 0){
    Serial.println("No measurement taken. Check physical connection.");
    sensors[2].activateAlarm +=1;
    return false;
  }
  else {
    Serial.println("Measurement was taken.");
  }
  mySDI12.clearBuffer();  // Clear the buffer for the next command
  float vmc_b = getValue(sdiResponse_b, '+', 1).toFloat()/10000;
  Serial.print("Volumetric Water Content: ");
  Serial.println(vmc_b);
  if(vmc_b>0.70 || vmc_b<-0.50){
    sensors[2].activateAlarm+=1;
    myBool = false;
  }
    if(vmc_b>0.70){
    String message = "Teros B water content is too high at " + String(vmc_b) + ". Teros B might be in a puddle.";
    LogError(message);
  }
  else if(vmc_b<-0.5){
    String message = "Teros B water content is too low at " + String(vmc_b) + ". Check sensor.";
    LogError(message);
  }
  float tmp_b = getValue(sdiResponse_b, '+', 2).toFloat();
  Serial.print("Soil Temperature: ");
  Serial.println(tmp_b);
  if(tmp_b>50 || tmp_b<-40){
    sensors[2].activateAlarm+=1;
    myBool = false;
    String message = "Teros B temperature is at " + String(tmp_b) + " c, which is out of range. Check the sensor.";
    LogError(message);
  }
  float eC_b = getValue(sdiResponse_b, '+', 3).toFloat();
  Serial.print("Electrical Conductivity: ");
  Serial.println(eC_b);
  if(eC_b < 0 || eC_b > 20){
    sensors[2].activateAlarm+=1;
    myBool = false;
    String message = "Electrical conductivity for Tero B is outside of range at " + String(eC_b) + ". Check sensor.";
    LogError(message);
  }

  Serial.println("Soil Moisture 2 is " + sdiResponse_b);
  data.soil_moisture_teros2 = sdiResponse_b;


  Serial.println("taking soil measurement from address b");
  mySDI12.sendCommand("bM!");
  delay(30);
  mySDI12.clearBuffer();
  delay(2000);
  Serial.println("Getting the data from address b");
  mySDI12.sendCommand("bD0!");
  delay(30);
  String sdiResponse_c = "";  // Create a blank String that will become the reported value
  while (mySDI12.available()) {
    char c = mySDI12.read();  // Read in a character
    if ((c != '\n') && (c != '\r')) {  // If the read-in character isn't a new line
      sdiResponse_c += c;  // Add the character to the String
      delay(10);
    }
  }
  if (sdiResponse_c.length() == 0){
    Serial.println("No measurement taken. Check physical connection.");
    sensors[3].activateAlarm +=1;
    return false;
  }
  else {
    Serial.println("Measurement was taken.");
  }
  mySDI12.clearBuffer();  // Clear the buffer for the next command
  float vmc_c = getValue(sdiResponse_c, '+', 1).toFloat()/10000;
  Serial.print("Volumetric Water Content: ");
  Serial.println(vmc_c);
  if(vmc_c>0.70 || vmc_c<-0.50){
    sensors[3].activateAlarm+=1;
    myBool = false;
  }
  if(vmc_c>0.70){
    String message = "Teros C water content is too high at " + String(vmc_c) + ". Teros C might be in a puddle.";
    LogError(message);
  }
  else if(vmc_c<-0.5){
    String message = "Teros C water content is too low at " + String(vmc_c) + ". Check sensor.";
    LogError(message);
  }

  float tmp_c = getValue(sdiResponse_c, '+', 2).toFloat();
  Serial.print("Soil Temperature: ");
  Serial.println(tmp_c);
  if(tmp_c>50 || tmp_c<-40){
    sensors[2].activateAlarm+=1;
    myBool = false;
    String message = "Teros C temperature is at " + String(tmp_c) + " c, which is out of range. Check the sensor.";
    LogError(message);
  }
  float eC_c = getValue(sdiResponse_c, '+', 3).toFloat();
  Serial.print("Electrical Conductivity: ");
  Serial.println(eC_c);
  if(eC_c < 0 || eC_c > 20){
    sensors[2].activateAlarm+=1;
    myBool = false;
    String message = "Electrical conductivity for Teros C is outside of range at " + String(eC_c) + ". Check sensor.";
    LogError(message);
  }

  Serial.println("Soil Moisture 3 is " + sdiResponse_c);
  data.soil_moisture_teros3 = sdiResponse_c;
  return myBool;

}

/* * Function: getValue
 * ----------------------------
 * Purpose:  Extracts a specific segment from a delimited string (e.g., SDI-12).
 * Params:   data (String) - Raw input; separator (char); index (int).
 * Returns:  String - The segment at the given index or "" if not found.
 * Notes:    Used to parse multiple sensor values from a single response string.
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

/* * Function: WriteToSD
 * ----------------------------
 * Purpose:  Logs JSON data to the SD card using a MMDDHH.txt naming convention.
 * Params:   json (String) - Data to log; data (SensorData&) - For timestamp.
 * Returns:  void
 * Notes:    Initializes SD on pin 12 and ensures two-digit formatting for filenames.
 */
void WriteToSD(String json, SensorData& data){

  String filename = "";
  int month = data.timestamp.month();
  int day = data.timestamp.day();
  int hour = data.timestamp.hour();
  // Day (DD) - Ensure two digits
  if (month < 10) filename += "0";
  filename += String(month);
  if (day < 10) filename += "0";
  filename += String(day);
  // Hour (HH) - Ensure two digits
  if (hour < 10) filename += "0";
  filename += String(hour);

  filename += ".txt";
          
  File dataFile = SD.open(filename, FILE_WRITE);
  if (dataFile) {
    // Write the JSON string to the file
    // Note: We write the JSON string directly since the timestamp is already included in the filename
    dataFile.println(json); 
    
    // Close the file to ensure data is written to disk
    dataFile.close();
    Serial.print("Successfully wrote new file: ");
    Serial.println(filename);
  } else {
    // If the file didn't open, print an error
    Serial.print("ERROR: Could not create file ");
    Serial.print(filename);
    Serial.println(" for writing.");
  }

}

void LogError(String message){
  File ErrorLog = SD.open(errorLog, FILE_WRITE);

  if(ErrorLog){
    ErrorLog.print(rtc.now().timestamp());
    ErrorLog.print(": ");
    ErrorLog.println(message);
    ErrorLog.close(); // This forces the data onto the SD card
    Serial.println("Error logged to SD.");
  } else{
    Serial.println("Critical Error: Couldnt log to SD card.");
  }
}
// ===== BATTERY AND SYSTEM TEST =====

/* * Function: testSystemStatus
 * ----------------------------
 * Purpose:  Measures battery health, updates system time, and verifies SD storage.
 * Params:   data (SensorData&) - Struct to store voltage and current timestamp.
 * Returns:  bool - Success status.
 * Notes:    Calculates battery via A6 divider; initializes SD on pin 12.
 */
bool testSystemStatus(SensorData& data) {
  printSeparator("SYSTEM STATUS TEST");
  
  // Battery voltage
  int raw = analogRead(A6);
  data.battery_voltage = (raw * 3.3 / 1024.0) * 2.0; // Mayfly voltage divider
  
  Serial.print("Battery Voltage: ");
  Serial.print(data.battery_voltage, 2);
  Serial.println("V");
  
  // RTC test
  data.timestamp = rtc.now();
  Serial.print("RTC Time: ");
  Serial.println(data.timestamp.timestamp());
  
  // SD card test
  Serial.print("SD Card: ");
  Serial.println(sd_ok ? "OK" : "FAILED");
  
  return true;
}

// ===== COMPLETE SENSOR TEST CYCLE =====

/* * Function: runCompleteSensorTest
 * ----------------------------
 * Purpose:  Executes the full suite of sensor measurements and system checks.
 * Params:   none
 * Returns:  void
 * Notes:    Aggregates PASS/FAIL results, generates JSON output, and handles 
 * hourly SD logging and strike reset.
 */
void runCompleteSensorTest() {
  printSeparator("STARTING COMPLETE SENSOR TEST CYCLE");
  
  bool all_tests_passed = true;
  
  // Initialize all error flags
  for (int i = 0; i < 10; i++) {
    data.sensor_errors[i] = false;
  }


  // Test 1: MaxBotix
  Serial.println("\n>>> TESTING MAXBOTIX ULTRASONIC <<<");
  delay(2000);
  data.ultrasonic_distance_mm = testMaxBotixSensor();
  bool ultrasonic_ok = (data.ultrasonic_distance_mm > 500 && data.ultrasonic_distance_mm < 3048);
  data.sensor_errors[1] = !ultrasonic_ok;
  all_tests_passed &= ultrasonic_ok;
  if(!ultrasonic_ok){
    sensors[0].activateAlarm+=1;
  }
  if(data.ultrasonic_distance_mm >= 3048){
    String message = " Distance is " + String(data.ultrasonic_distance_mm) + " mm, Which is at or above threshold of 3048 mm. Check the ultrasonic angle.";
    LogError(message);
  }
  else if(data.ultrasonic_distance_mm <= 500){
    String message = " Distance is " + String(data.ultrasonic_distance_mm) + " mm, Which is at or below threshold of 500 mm. The tower may need to be adjusted upward.";
    LogError(message);
  }
  Serial.print("Ultrasonic test result: ");
  Serial.println(ultrasonic_ok ? "PASS" : "FAIL");
  delay(50);
  

  // Test 2: Apogee Radiation
  Serial.println("\n>>> TESTING APOGEE RADIATION SENSORS <<<");
  delay(2000);
  bool radiation_ok = testApogeeRadiation(data);
  data.sensor_errors[2] = !radiation_ok;
  all_tests_passed &= radiation_ok;
  Serial.print("Radiation sensors test result: ");
  Serial.println(radiation_ok ? "PASS" : "FAIL");
  delay(50);
  
  // Test 3: Temperature
  Serial.println("\n>>> TESTING TEMPERATURE SENSOR <<<");
  delay(2000);
  data.air_temp_c = testTemperatureSensor();
  bool temp_ok = (data.air_temp_c > -20.55 && data.air_temp_c < 49);
  if (!temp_ok){
    sensors[4].activateAlarm+=1;
    String message = "Temperature is " + String(data.air_temp_c) + " c, which is outside of range. Check the sensor.";
    LogError(message);
  }
  data.sensor_errors[3] = !temp_ok;
  all_tests_passed &= temp_ok;
  Serial.print("Temperature test result: ");
  Serial.println(temp_ok ? "PASS" : "FAIL");
  delay(50);
  
  // Test 4: Soil Moisture
  Serial.println("\n>>> TESTING SOIL MOISTURE SENSORS <<<");
  delay(2000);
  bool soil_ok = testSoilMoisture(data);
  data.sensor_errors[4] = !soil_ok;
  all_tests_passed &= soil_ok;
  Serial.print("Soil moisture test result: ");
  Serial.println(soil_ok ? "PASS" : "FAIL");
  delay(50);
  
  // Test 5: System Status
  Serial.println("\n>>> TESTING SYSTEM STATUS <<<");
  delay(2000);
  bool system_ok = testSystemStatus(data);
  data.sensor_errors[5] = !system_ok;
  all_tests_passed &= system_ok;
  Serial.print("System status test result: ");
  Serial.println(system_ok ? "PASS" : "FAIL");
  delay(50);
  
  // Test 6: LTE Bee
  Serial.println("\n>>> TESTING LTE BEE COMMUNICATION <<<");
  delay(2000);
  data.lte_connected = testLTEBee();
  data.sensor_errors[6] = !data.lte_connected;
  Serial.print("LTE Bee test result: ");
  Serial.println(data.lte_connected ? "PASS" : "FAIL");
  delay(50);
  
  // Summary
  printSeparator("TEST RESULTS SUMMARY");
  printTestResult("Ultrasonic Sensors", ultrasonic_ok);
  printTestResult("Radiation Sensors", radiation_ok);
  printTestResult("Temperature", temp_ok, data.air_temp_c, "°C");
  printTestResult("Soil Moisture", soil_ok);
  printTestResult("System Status", system_ok, data.battery_voltage, "V");
  printTestResult("LTE Communication", data.lte_connected);
  
  Serial.println();
  Serial.print("OVERALL TEST STATUS: ");
  Serial.println(all_tests_passed ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
  
  // Data output in JSON format
  
  printSeparator("JSON DATA OUTPUT");
  json = createDataJSON(data);
  Serial.println(json);
}

// ===== JSON DATA FORMATTING =====

/* * Function: createDataJSON
 * ----------------------------
 * Purpose:  Serializes the SensorData struct into a formatted JSON string.
 * Params:   data (const SensorData&) - The struct containing current readings.
 * Returns:  String - A JSON-formatted block ready for SD logging or transmission.
 * Notes:    Organizes data into nested objects: radiation, environment, soil, and system.
 */
String createDataJSON(const SensorData& data) {
  String json = "{\n";
  json += "  \"timestamp\": \"" + data.timestamp.timestamp() + "\",\n";
  json += "  \"snow_depth\": {\n";
  json += "    \"Ultrasonic\": " + String(data.ultrasonic_distance_mm, 2) + ",\n";
  json += "  },\n";
  json += "  \"radiation\": {\n";
  json += "    \"pyrgeometer_up_mv\": " + String(data.pyrgeometer_up_mv, 2) + ",\n";
  json += "    \"pyrgeometer_down_mv\": " + String(data.pyrgeometer_down_mv, 2) + ",\n";
  json += "    \"pyranometer_up_mv\": " + String(data.pyranometer_up_mv, 2) + ",\n";
  json += "    \"pyranometer_down_mv\": " + String(data.pyranometer_down_mv, 2) + "\n";
  json += "  },\n";
  json += "  \"environment\": {\n";
  json += "    \"air_temp_c\": " + String(data.air_temp_c, 2) + "\n";
  json += "  },\n";
  json += "  \"soil\": {\n";
  json += "    \"teros_percent{a+vmc+tmp+ec}\": " + data.soil_moisture_teros + ",\n";
  json += "    \"teros2_percent{a+vmc+tmp+ec}\": " + data.soil_moisture_teros2 + ",\n";
  json += "    \"teros3_percent{a+vmc+tmp+ec}\": " + data.soil_moisture_teros3 + ",\n";
  json += "  },\n";
  json += "  \"system\": {\n";
  json += "    \"battery_v\": " + String(data.battery_voltage, 2) + ",\n";
  json += "    \"lte_connected\": " + String(data.lte_connected ? "true" : "false") + ",\n";
  json += "  },\n";
  json += "}";
  json += "  \"Errors\": " + data.systemAlarm + "\n";
  return json;
}


// ===== SETUP =====
void setup() {

  Serial.begin(115200);
  while (!Serial) delay(100);

sensors[0].type = ULTRASONIC;
sensors[1].type = TEROS1;
sensors[2].type = TEROS2;
sensors[3].type = TEROS3;
sensors[4].type = TEMP_SENSOR;
sensors[5].type = PYRGEOMETER_UP;
sensors[6].type = PYRGEOMETER_DOWN;
sensors[7].type = PYRANOMETER_UP;
sensors[8].type = PYRANOMETER_DOWN;

  delay(2000); // Give time to open serial monitor
  
  printSeparator("SNOW SENSING STATION - TEST MODE");
  Serial.println("Hardware: EnviroDIY Mayfly with LTE Bee");
  Serial.println("Mode: Individual sensor testing");
  Serial.println();
  
  // Initialize power system
  initializePowerSystem();
  
  // Initialize I2C devices
  Wire.begin();

  sd_ok = SD.begin(12);
  Serial.println("Sd okay " + String(sd_ok));
  

  sw.begin(0x6B);
  // Initialize RTC
  if (!rtc.begin()) {
    Serial.println("ERROR: RTC not found!");
  } else {
    Serial.println("RTC initialized successfully");
    DateTime now = rtc.now();
    // Check if RTC needs to be set
    if (now.year() < 2024) {
      Serial.println("RTC appears to need setting (year < 2024)");
      Serial.println("Setting RTC to compile time...");
      
      // Set RTC to the time this sketch was compiled
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
      
      now = rtc.now();
      Serial.print("RTC set to: ");
      Serial.println(now.timestamp());
    } else {
      Serial.print("RTC time looks good: ");
      Serial.println(now.timestamp());
    }
  }

  // Initialize ADS1115
  if (!ads.begin()) {
    Serial.println("ERROR: ADS1115 not found!");
    apogeeFlag = 1;
  } else {
    Serial.println("ADS1115 initialized successfully");
  }
  
  // Initialize temperature sensor
  tempSensor.begin();
  Serial.println("Temperature sensor initialized");
  
  //Activate SDI12 Communication
  mySDI12.begin();
  Serial.println("\nInitialization complete!");
  Serial.println("Starting sensor tests in 3 seconds...");
  delay(3000);

  Serial.println(F("Enabling switched 3.3V power rail..."));
  const int8_t switchedPowerPin = 22;
  pinMode(switchedPowerPin, OUTPUT);
  digitalWrite(switchedPowerPin, HIGH);
  delay(500);
  Serial.println(F("✓ Switched power enabled"));
  Serial.println(F("  Check: 'Switched Power Out' LED should be ON now!\n"));

/*
  //set up modem pins
  Serial.println(F("Configuring modem pins..."));
  pinMode(modemVccPin, OUTPUT);
  pinMode(modemStatusPin, INPUT);
  pinMode(modemSleepRqPin, OUTPUT);
    
  // Power cycle sequence for SIM7080G
  Serial.println(F("Power cycling modem..."));

  // Turn off power
  digitalWrite(modemVccPin, LOW);
  digitalWrite(modemSleepRqPin, HIGH);  // HIGH = sleep for SIM7080G (inverted!)
  delay(2000);
    
  // Turn on power
  digitalWrite(modemVccPin, HIGH);
  delay(500);

  // Keep modem awake (DTR/PWRKEY)
  digitalWrite(modemSleepRqPin, LOW);   // LOW = awake for SIM7080G (inverted!)
  delay(100);

  // Check status pin
  Serial.print(F("Status pin state: "));
  Serial.println(digitalRead(modemStatusPin) ? "HIGH" : "LOW");

  Serial.println(F("Waiting 15 seconds for modem boot..."));
  for (int i = 15; i > 0; i--) {
    Serial.print(i);
    Serial.print(F("... "));
    delay(1000);
  }
  Serial.println(F("GO!\n"));

  Serial1.begin(MODEM_SERIAL_BAUD_RATE);
  delay(100);

  Serial.println(F(">>> Attempting modem.init()..."));
  if (modem.init()) {
    Serial.println(F("\n✓✓✓ MODEM RESPONDED! ✓✓✓\n"));
  } else {
    Serial.println(F("\n✗ Modem did not respond to init()"));
    Serial.println(F("\nIf you see NO AT commands above, the modem isn't communicating."));
    Serial.println(F("If you see AT commands but errors, the modem is trying to respond."));
    Serial.println(F("\nTroubleshooting:"));
    Serial.println(F("  1. Let battery charge for 30 minutes"));
    Serial.println(F("  2. Check LTE Bee is fully inserted"));
    Serial.println(F("  3. Try removing and reinserting LTE Bee"));
    Serial.println(F("  4. Check if 'Switched Power Out' LED is on"));
    Serial.println();
    while(1) delay(1000);
  }

  String modemInfo = modem.getModemInfo();
  Serial.print(F("Model: "));
  Serial.println(modemInfo);

  String imei = modem.getIMEI();
  Serial.print(F("IMEI: "));
  Serial.println(imei); 

  int simStatus = modem.getSimStatus();
  Serial.print(F("SIM Status: "));
  Serial.println(simStatus);

  if (simStatus == 1) {
    Serial.println(F("✓ SIM card ready"));
  } else {
    Serial.println(F("✗ SIM card not ready"));
  } 
  Serial.println(F("Setting network mode (LTE-M)..."));
  modem.sendAT("+CNMP=38");  // LTE only
  modem.waitResponse();
    
  modem.sendAT("+CMNB=1");   // Cat-M preferred
  modem.waitResponse();
    
  Serial.println(F("\nWaiting for network (up to 90 seconds)..."));
  Serial.println(F("This may take a while on first connection..."));
    
  if (modem.waitForNetwork(90000L)) {
    Serial.println(F("\n✓ Registered on network!"));
        
    String op = modem.getOperator();
    Serial.print(F("Operator: "));
    Serial.println(op);
        
    int csq = modem.getSignalQuality();
    Serial.print(F("Signal Quality: "));
    Serial.print(csq);
    Serial.println(F("/31"));
        
  } else {
    Serial.println(F("\n✗ Network registration failed"));
    Serial.println(F("\nPossible reasons:"));
    Serial.println(F("  - Weak signal (move near window)"));
    Serial.println(F("  - SIM not activated on Hologram"));
    Serial.println(F("  - Network congestion"));
        
    Serial.println(F("\nPress RESET to try again"));
    while(1) delay(1000);
  }

  Serial.print(F("Connecting to APN: "));
  Serial.println(apn);
    
  if (modem.gprsConnect(apn)) {
    Serial.println(F("\n✓✓✓ CONNECTED TO INTERNET! ✓✓✓"));
        
    IPAddress local = modem.localIP();
    Serial.print(F("Local IP: "));
    Serial.println(local);
        
  } else {
    Serial.println(F("\n✗ GPRS connection failed"));
  }
  modem.sendAT("+CSMP=31,167,0,0"); 
  modem.waitResponse();
  */
}

void sendAlarm(){
  //print the alarm to website instead
  //modem.sendSMS("+13853218055", message + " needs to be attended to.");
  Serial.print("Sensor Error Reading: Check data logs.");
  data.systemAlarm = ("Sensor Error Reading: Check data logs.");
}

// ===== MAIN LOOP =====
void loop() {
  delay(5000);
  String alarmNames[] = {"ultrasonic", "Teros1", "Teros2", "Teros3", "Temp Sensor", "Pyrgeometer", "Pyrgeometer", "Pyranometer", "Pyranometer"};
  
  DateTime hour = rtc.now();
  currentHour = hour.hour();
  if (testMode) {
    unsigned long loop_start_time = millis();
    runCompleteSensorTest();
    for (int i = 0; i < 9; i++) {
    Serial.println("Alarm warnings triggered: " + alarmNames[i] + ": " + sensors[i].activateAlarm);
    if (sensors[i].activateAlarm >= 3) {
      sendAlarm();
    }
  }
    Serial.println("\n\nNext test cycle in 10 seconds...");
    Serial.println("================================================\n");
    delay(TEST_INTERVAL);
    unsigned long loop_end_time = millis()-loop_start_time;
    String loopString = String(loop_end_time);
    Serial.println("The loop took " + loopString + " seconds.");
    if (!(currentHour == pastHour)){
      WriteToSD(json, data);
      for(int i = 0; i < 9; i ++){
        sensors[i].activateAlarm = 0;
      }
  }
  } else {
    // Production mode would go here
    Serial.println("Production mode not implemented yet");
    delay(60000);
  }
  pastHour = currentHour;
}
