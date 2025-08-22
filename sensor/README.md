# ESP32 Sensor

## Prerequisites
- PlatformIO CLI (`pip install platformio`)
- ESP32

## Setup

### 1. WiFi Credentials

WiFi credentials managed with `.env` file. The Makefile automatically loads `.env` when building.

### 2. Configuration

Configuration managed with `data/config.json`.

Configuration options:
- `wifi_enabled` - Enable/disable WiFi
- `wifi_http_enabled` - Enable/disable HTTP POST to server
- `wifi_http_url` - Your server endpoint URL
- `ble_enabled` - Enable/disable Bluetooth
- `web_server_enabled` - Enable/disable web interface
- `data_logging_enabled` - Enable/disable SPIFFS CSV logging
- `system_status_enabled` - Enable/disable status logging
- `sensor_interval_ms` - Time between sensor readings (milliseconds)

Upload to ESP32 with `make uploadfs` (only needed when config changes).

## Commands

```bash
make build     # Compile the project
make upload    # Upload to ESP32
make monitor   # Open serial monitor
make clean     # Clean build files
make all       # Build, upload and monitor
```

## Serial Configuration
- Baud Rate: 115200 

## Data Logging

Sensor data is automatically logged to SPIFFS (internal flash) as a CSV file at `/sensor_data.csv`. 

- [ ] Aggressive Power Optimization (for battery operation)
  - Implement deep sleep mode between readings (1-2µA sleep current vs 200mA active)
  - Configure wake-up intervals (e.g., every 5 minutes instead of continuous 5-second loop)
  - Batch sensor data in SPIFFS instead of sending immediately
  - Periodic bulk upload (e.g., wake up every hour to send all buffered data)
  - WiFi: Connect only when needed for data upload, then disconnect
  - BLE: Make optional/disable for battery mode
  - Web server: Disable during deep sleep, only enable on-demand
  - Reduce CPU frequency from 240MHz to 80MHz
  - Add power mode configuration (AC_POWERED vs BATTERY_POWERED)
  - Implement RTC memory for persisting data across deep sleep cycles
  - Expected battery life improvement: 4-8 hours → Days to weeks

- [ ] Battery monitoring 
  - External battery monitoring 
  - Add voltage divider circuit with two resistors to scale battery down to esp32 3.3v range
  - Read adc voltage from voltage divider pin 
  - Convert to battery voltage, caculate percentage 
  - Low battery, sleeping warning 

- [ ] ADC Voltage Reference Calibration
  - **Issue**: ADC calculations assume 3.3V reference, but actual supply voltage may vary (especially on battery)
  - Current code: `voltage = adcReading * (3.3 / 4095.0)` assumes fixed 3.3V
  - If battery supplies 3.0V, readings will be 10% too high
  - **Solutions**:
    1. Measure actual supply voltage via voltage divider and use as Vref in calculations
    2. Use ESP32's internal voltage reference measurement (less accurate, ±10%)
    3. Add external precision voltage reference IC (TL431, REF3030)
    4. If using ratiometric sensors (sensor powered from same rail), error may cancel out
  - Recommended: Implement solution #1 when adding battery voltage monitoring


### Notes

- Major Power Drains:
  - WiFi (Biggest consumer)
    - ~100-200mA when active and connected
    - Always on, never sleeps
    - HTTP POST every 5 seconds
  - BLE
    - ~20-50mA when advertising
    - Constantly advertising