# Snow Sensing Station – Arduino (EnviroDIY Mayfly)

## Overview

This project implements a **Snow Sensing Station** using the **EnviroDIY Mayfly** data logger paired with a **SIM7080 LTE Bee**. The system periodically reads environmental sensor data, logs it to an SD card, and formats the data as JSON for debugging or future cellular transmission.

The code is written to support modular sensor handling, time-based logging, and alarm management.

---

## Software Requirements

- Arduino IDE
- EnviroDIY Mayfly board support package
- Required libraries:
  - TinyGSM
  - SDI12
  - ArduinoJson
  - Wire
  - SPI

### Modem Configuration (Required)

The following definitions **must appear before all library includes** in the `.ino` file:

```cpp
#define TINY_GSM_MODEM_SIM7080
#define TINY_GSM_RX_BUFFER 64
#define TINY_GSM_YIELD() { delay(2); }