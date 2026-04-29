/*
 The purpose of this file is to run all the tests for the sensors and systems used by the MayFly DataLogger.
 */

#ifndef TESTING_H
#define TESTING_H
#include "snow_station_definitions.h"
#include "Documentation.h"



/* * Function: testSystemStatus
 * ----------------------------
 * Purpose:  Measures battery health, updates system time, and verifies SD storage.
 * Params:   data (SensorData&) - Struct to store voltage and current timestamp.
 * Returns:  bool - Success status.
 * Notes:    Calculates battery via A6 divider; initializes SD on pin 12.
 */
bool testSystemStatus(SensorData& data);

/* * Function: testLTEBee
 * ----------------------------
 * Purpose:  Verifies hardware power and simulated AT command sequence for the SIM7080G.
 * Params:   none
 * Returns:  bool - Success status (assumed true for test mode).
 * Notes:    Configures Hologram APN and checks SIM/Network registration status.
 */
bool testLTEBee();

/* * Function: testMaxBotixSensor
 * ----------------------------
 * Purpose:  Measures distance via SerialWombat UART and updates alarm strikes.
 * Params:   none
 * Returns:  float - Measured distance in mm; returns -1 if sensor fails to respond.
 * Notes:    Increments sensors[0].activateAlarm if reading is out of bounds (>3048 or <508).
 * Strike count persists until the hourly reset logic in the main loop.
 */
float testMaxBotixSensor();

/* * Function: testApogeeRadiation
 * ----------------------------
 * Purpose:  Reads 4 channels of radiation data (mV) via ADS1115 ADC.
 * Params:   data (SensorData&) - Struct to store calculated millivolt values.
 * Returns:  bool - True once all 4 channels are sampled.
 * Notes:    Increments alarm strikes if a 0mV reading is detected during
 * daylight hours (10:00 to 17:00). Strikes persist until hourly reset.
 */
bool testApogeeRadiation(SensorData& data);

/* * Function: testTemperatureSensor
 * ----------------------------
 * Purpose:  Reads air temperature from the DS18B20 OneWire digital sensor.
 * Params:   none
 * Returns:  float - Temperature in Celsius; returns -999 if disconnected.
 * Notes:    Increments sensors[4].activateAlarm if temp is out of realistic
 * range (>49°C or <-20.55°C) or if the device is disconnected.
 * Strikes persist until the hourly reset in the main loop.
 */
float testTemperatureSensor();

/* * Function: testSoilMoisture
 * ----------------------------
 * Purpose:  Queries two Teros-12 soil sensors via SDI-12 protocol
 * on addresses '1' and 'b'
 * Params:   data (SensorData&) - Struct to store the raw SDI-12 response strings.
 * Returns:  bool - Returns true if measurements are successful, false if a sensor fails.
 * Notes:    Increments alarm strikes (sensors[1], [2], or [3]) if a sensor
 * fails to return a string. Strikes persist until the hourly reset.
 */
bool testSoilMoisture(SensorData& data);

/* * Function: getValue
 * ----------------------------
 * Purpose:  Extracts a specific segment from a delimited string (e.g., SDI-12).
 * Params:   data (String) - Raw input; separator (char); index (int).
 * Returns:  String - The segment at the given index or "" if not found.
 * Notes:    Used to parse multiple sensor values from a single response string.
 */
String getValue(String data, char separator, int index);

/* * Function: runCompleteSensorTest
 * ----------------------------
 * Purpose:  Executes the full suite of sensor measurements and system checks.
 * Params:   none
 * Returns:  void
 * Notes:    Aggregates PASS/FAIL results, generates JSON output, and handles
 * hourly SD logging and strike reset.
 */
void runCompleteSensorTest();

#endif
