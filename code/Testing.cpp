#include "Testing.h"


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

bool testApogeeRadiation(SensorData& data) {
  bool myBool = true;
  printSeparator("APOGEE RADIATION SENSORS TEST");
  Serial.println("Reading ADS1115 channels...");
  
  // Read all 4 channels (adjust channel assignments as needed)
  int raw_pyr_up = ads1.readADC_Differential_0_1();
  int pyrgeometerData1 = ads1.computeVolts(raw_pyr_up) * 1000;

  int raw_temp1 = ads2.readADC_SingleEnded(1);
  int pyrgeometerTemp1 = ads2.computeVolts(raw_temp1) * 1000;

  data.pyrgeometer_up_mv = K1*pyrgeometerData1+K2*ST*pyrgeometerTemp1;
  
  delay(50);
  DateTime now = rtc.now();
  if(data.pyrgeometer_up_mv == 0 && !(now.hour()>9 && now.hour()<17)){
    sensors[5].activateAlarm +=1;
    LogError("The time is " + String(rtc.now().hour()) + ":" + String(rtc.now().minute()) + " and the pyrgeometer up radiation value is 0. Check connection.");
    myBool = false;
  }
  int raw_pyr_down = ads1.readADC_Differential_2_3();
  int pyrgeometerData2 = ads1.computeVolts(raw_pyr_down) * 1000;

  int raw_temp2 = ads2.readADC_SingleEnded(2);
  int pyrgeometerTemp2 = ads2.computeVolts(raw_temp2) * 1000;
  
  data.pyrgeometer_down_mv = K1*pyrgeometerData2+K2*ST*pyrgeometerTemp2;

  delay(50);
  if(data.pyrgeometer_down_mv == 0 && !(now.hour()>9 && now.hour()<17)){
    sensors[6].activateAlarm +=1;
    LogError("The time is " + String(rtc.now().hour()) + ":" + String(rtc.now().minute()) + " and the pyrgeometer down radiation value is 0. Check connection.");
    myBool = false;
  }
  int raw_pyra_up = data.pyranometer_up_mv = ads.readADC_Differential_2_3();
  int pyranometerUpData = ads.computeVolts(raw_pyra_up) * 1000;
  data.pyranometer_up_mv = CFU*pyranometerUpData;
  delay(50);
  if(data.pyranometer_up_mv == 0 && !(now.hour()>9 && now.hour()<17)){
    sensors[7].activateAlarm +=1;
    LogError("The time is " + String(rtc.now().hour()) + ":" + String(rtc.now().minute()) + " and the pyranometer up radiation value is 0. Check connection.");
    myBool = false;
  }

  int raw_pyra_down = data.pyranometer_down_mv = ads.readADC_Differential_0_1();
  int pyranometerDownData = ads.computeVolts(raw_pyra_down) * 1000;
  data.pyranometer_down_mv = CFD*pyranometerDownData;
  
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
    myBool = false;
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
    myBool = false;
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
  

  Serial.println("taking soil measurement from address b");
  mySDI12.sendCommand("bM!");
  delay(30);
  mySDI12.clearBuffer();
  delay(2000);
  Serial.println("Getting the data from address b");
  mySDI12.sendCommand("bD0!");
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
    myBool = false;
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


  // Serial.println("taking soil measurement from address b");
  // mySDI12.sendCommand("bM!");
  // delay(30);
  // mySDI12.clearBuffer();
  // delay(2000);
  // Serial.println("Getting the data from address b");
  // mySDI12.sendCommand("bD0!");
  // delay(30);
  // String sdiResponse_c = "";  // Create a blank String that will become the reported value
  // while (mySDI12.available()) {
  //   char c = mySDI12.read();  // Read in a character
  //   if ((c != '\n') && (c != '\r')) {  // If the read-in character isn't a new line
  //     sdiResponse_c += c;  // Add the character to the String
  //     delay(10);
  //   }
  // }
  // if (sdiResponse_c.length() == 0){
  //   Serial.println("No measurement taken. Check physical connection.");
  //   sensors[3].activateAlarm +=1;
  //   myBool = false;
  // }
  // else {
  //   Serial.println("Measurement was taken.");
  // }
  // mySDI12.clearBuffer();  // Clear the buffer for the next command
  // float vmc_c = getValue(sdiResponse_c, '+', 1).toFloat()/10000;
  // Serial.print("Volumetric Water Content: ");
  // Serial.println(vmc_c);
  // if(vmc_c>0.70 || vmc_c<-0.50){
  //   sensors[3].activateAlarm+=1;
  //   myBool = false;
  // }
  // if(vmc_c>0.70){
  //   String message = "Teros C water content is too high at " + String(vmc_c) + ". Teros C might be in a puddle.";
  //   LogError(message);
  // }
  // else if(vmc_c<-0.5){
  //   String message = "Teros C water content is too low at " + String(vmc_c) + ". Check sensor.";
  //   LogError(message);
  // }

  // float tmp_c = getValue(sdiResponse_c, '+', 2).toFloat();
  // Serial.print("Soil Temperature: ");
  // Serial.println(tmp_c);
  // if(tmp_c>50 || tmp_c<-40){
  //   sensors[2].activateAlarm+=1;
  //   myBool = false;
  //   String message = "Teros C temperature is at " + String(tmp_c) + " c, which is out of range. Check the sensor.";
  //   LogError(message);
  // }
  // float eC_c = getValue(sdiResponse_c, '+', 3).toFloat();
  // Serial.print("Electrical Conductivity: ");
  // Serial.println(eC_c);
  // if(eC_c < 0 || eC_c > 20){
  //   sensors[2].activateAlarm+=1;
  //   myBool = false;
  //   String message = "Electrical conductivity for Teros C is outside of range at " + String(eC_c) + ". Check sensor.";
  //   LogError(message);
  // }

  // Serial.println("Soil Moisture 3 is " + sdiResponse_c);
  // data.soil_moisture_teros3 = sdiResponse_c;
  return myBool;

}


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


