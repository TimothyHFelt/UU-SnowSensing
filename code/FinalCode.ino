/*
 * Snow Sensing Station - Complete Sensor Test Mode
 * Tests all sensors individually with clear serial output
 * For EnviroDIY Mayfly with LTE Bee cellular communication
 */

#include "snow_station_definitions.h"
#include "Testing.h"
#include "Documentation.h"

//Flags
volatile bool hourly_write_pending = false;
int apogeeFlag = 0;
bool testMode = true;
bool sd_ok;

//serial Wombat Initialize
SerialWombatChip sw;
SerialWombatUART myUart(sw);

//Initialize the Json and bool
String json;

//Modem initialization
TinyGsm modem(debugger);

//Initialize Temp Sensor
OneWire oneWire;
SDI12 mySDI12(7);
DallasTemperature tempSensor(&oneWire);

//Real Time Clock
RTC_DS3231 rtc;

//Analog to Digital Converters
Adafruit_ADS1115 ads;  // ADS1115 for Apogee sensors
Adafruit_ADS1115 ads1;
Adafruit_ADS1115 ads2;

//time keeping 
int pastHour = -1;
int currentHour = -1;

String myBuffer = "";
int textCount = 0;

StreamDebugger debugger(Serial1, Serial);

//Initialize Sensor Data
SensorData data;

//Initialize Sensor Alarms
SensorStatus sensors[10];


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
  if (!ads.begin(0x48)){
    Serial.println("ERROR: ADS1115 onboard chip not found!");
    apogeeFlag = 1;
  } else {
    Serial.println("ADS1115 onboard chip initialized successfully");
    ads1.setGain(GAIN_ONE);
  }
  // Initialize ADS1115
  if (!ads1.begin(0x4A)) {
    Serial.println("ERROR: ADS1115 board 1 not found!");
    apogeeFlag = 1;
  } else {
    Serial.println("ADS1115 Board 1 initialized successfully");
    ads1.setGain(GAIN_ONE);
  }
  
  if (!ads2.begin(0x49)) {
    Serial.println("ERROR: ADS1115 board 2 not found!");
    apogeeFlag = 1;
  } else {
    Serial.println("ADS1115 Board 2 initialized successfully");
    ads1.setGain(GAIN_SIXTEEN);
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
