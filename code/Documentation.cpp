#include "Documentation.h"

void printSeparator(const char* title) {
  Serial.println();
  Serial.println("================================================");
  Serial.print("  ");
  Serial.println(title);
  Serial.println("================================================");
  delay(2000); // 2 second pause after each section header
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
  // json += "    \"teros3_percent{a+vmc+tmp+ec}\": " + data.soil_moisture_teros3 + ",\n";
  json += "  },\n";
  json += "  \"system\": {\n";
  json += "    \"battery_v\": " + String(data.battery_voltage, 2) + ",\n";
  json += "    \"lte_connected\": " + String(data.lte_connected ? "true" : "false") + ",\n";
  json += "  },\n";
  json += "}";
  json += "  \"Errors\": " + data.systemAlarm + "\n";
  return json;
}

void sendAlarm(){
  //print the alarm to website instead
  //modem.sendSMS("+13853218055", message + " needs to be attended to.");
  Serial.print("Sensor Error Reading: Check data logs.");
  data.systemAlarm = ("Sensor Error Reading: Check data logs.");
}

