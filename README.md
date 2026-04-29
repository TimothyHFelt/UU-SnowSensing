# Snow Sensing Station - Technical Overview

This repository contains the complete firmware for the **Snow Sensing Station**, an advanced environmental monitoring system designed for the **EnviroDIY Mayfly DataLogger**. [cite_start]The station performs high-precision measurements of snow depth, atmospheric radiation, temperature, and soil moisture, with integrated LTE cellular communication and automated error logging[cite: 1, 16, 17].

---

## 🏗️ System Architecture

The project is structured as a robust, modular embedded system with dedicated handlers for sensing, documentation, and hardware testing.

### 1. Core Monitoring Logic
The station operates in a **Test Mode** or **Production Mode**. In the current implementation, it executes a complete sensor test cycle every 10 seconds. 
* **Safety Heartbeat:** The system tracks "alarm strikes" for every sensor. If a sensor fails or returns out-of-range data three times, a system-wide alarm is triggered.
* **Hourly Synchronization:** Data is serialized into JSON and written to an SD card every hour using an `MMDDHH.txt` naming convention to ensure data persistence.

### 2. Sensor Payload
The firmware integrates a diverse array of professional-grade scientific sensors:
| Sensor Category | Hardware / Protocol | Measurement |
| :--- | :--- | :--- |
| **Snow Depth** | MaxBotix Ultrasonic (via SerialWombat) | Distance in mm |
| **Radiation** | Apogee Pyranometers & Pyrgeometers | Upward/Downward short/long-wave radiation |
| **Soil Moisture** | Teros-12 (SDI-12 Protocol) | VMC, Soil Temp, and Electrical Conductivity |
| **Temperature** | DS18B20 (OneWire) | Air temperature in Celsius |
| **System** | ADC A6 Divider / RTC DS3231 | Battery health and precision timestamps |


---

## 🛠️ Hardware Stack & Definitions

* [cite_start]**Microcontroller:** EnviroDIY Mayfly[cite: 19].
* [cite_start]**Communication:** SIM7080G LTE Bee (Hologram APN)[cite: 35, 41, 55].
* [cite_start]**Power Management:** Utilizes a **latching relay** (Pins 10 & 11) to control the main sensor power rail, significantly reducing power consumption in the field[cite: 11, 13, 14].
* [cite_start]**Peripheral Expansion:** Uses an **ADS1115 ADC** for high-resolution analog radiation data and a **SerialWombat** chip for UART expansion[cite: 4, 20].

---

## 📂 File Structure

* **`snow_station_final.ino`**: The main entry point. Handles hardware initialization (RTC, SD, I2C, Modem) and the primary control loop.
* **`snow_station_definitions.h`**: Centralized configuration for hardware pins, sensor indices, and the `SensorData` global struct.
* **`Testing.cpp / .h`**: Contains the logic for individual sensor drivers, range validation, and the `runCompleteSensorTest` suite.
* **`Documentation.cpp / .h`**: Manages data serialization (JSON), SD card logging (FAT32), and terminal diagnostics.

---

## 🚦 Operational Safety Features

To prevent data corruption and hardware failure in remote locations, the station includes several "Self-Healing" features:
1.  **Context-Aware Error Logging:** If soil moisture is too high, the system logs a specific warning that the sensor "might be in a puddle".
2.  **Radiation Validation:** During daylight hours (10:00–17:00), the system checks if radiation sensors are returning 0mV, which indicates a connection failure rather than a low-light state.
3.  [cite_start]**Redundant Power Cycles:** The modem initialization includes a physical power cycle sequence using the `modemVccPin` and `modemSleepRqPin` to ensure the cellular module recovers from hang-ups[cite: 35, 36].


---

## 🚀 Deployment

1.  **Configure APN:** Ensure the `apn[]` in `snow_station_definitions.h` matches your cellular provider (default is "hologram").
2.  [cite_start]**RTC Calibration:** The firmware automatically adjusts the RTC to the compile time if it detects a lost power state (Year < 2024)[cite: 23, 24].
3.  **SD Initialization:** Insert a FAT32 formatted SD card; the system initializes the card on Pin 12.
