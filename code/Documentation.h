//the purpose of this function is to keep track print to the terminal, write or read from files, and log errors.
#ifndef DOCUMENTATION_H
#define DOCUMENTATION_H
#include "snow_station_definitions.h"

/* * Function: printSeparator
 * ----------------------------
 * Purpose:  Prints a formatted visual header to the Serial monitor to
 * organize sensor test sections during debugging.
 * * Params:   title (const char*) - The name of the section to display.
 * Returns:  void
 * * Notes:    Includes a 2-second delay to ensure the header is readable
 * before data begins to scroll.
 */
void printSeparator(const char* title);

/* * Function: LogError
 * ----------------------------
 * Purpose:  Writes a time-stamped error message to the SD card.
 * Params:   message (String) - The descriptive error text to be recorded.
 * Returns:  void
 * Notes:    Attempts to open the global 'errorLog' file for writing. If successful,
 * prepends a current RTC timestamp before the message and closes the
 * file to ensure data persistence.
 */
void LogError(String message);

/* * Function: printTestResult
 * ----------------------------
 * Purpose:  Standardizes Serial output for sensor tests with PASS/FAIL status.
 * Params:   sensor (const char*) - Name; success (bool); value (float); unit (const char*).
 * Returns:  void
 * Notes:    Only prints numerical value and unit if 'success' is true.
 */
void printTestResult(const char* sensor, bool success, float value = 0, const char* unit = "");

/* * Function: WriteToSD
 * ----------------------------
 * Purpose:  Logs JSON data to the SD card using a MMDDHH.txt naming convention.
 * Params:   json (String) - Data to log; data (SensorData&) - For timestamp.
 * Returns:  void
 * Notes:    Initializes SD on pin 12 and ensures two-digit formatting for filenames.
 */
void WriteToSD(String json, SensorData& data);

/* * Function: createDataJSON
 * ----------------------------
 * Purpose:  Serializes the SensorData struct into a formatted JSON string.
 * Params:   data (const SensorData&) - The struct containing current readings.
 * Returns:  String - A JSON-formatted block ready for SD logging or transmission.
 * Notes:    Organizes data into nested objects: radiation, environment, soil, and system.
 */
String createDataJSON(const SensorData& data);

/* * Function: sendAlarm
 * ----------------------------
 * Purpose:  Logs a system-wide error message and updates the global data structure.
 * Params:   none
 * Returns:  void
 * Notes:    Prints a status warning to the Serial monitor and populates
 * data.systemAlarm. SMS transmission is currently disabled.
 */
void sendAlarm()

#endif
