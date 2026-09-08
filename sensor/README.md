# ESP32 Soil Sensor

Reads a homemade gypsum block soil moisture sensor through a voltage divider,
logs each reading to flash, and reports it to the telemetry API over WiFi.

## Wiring

```
ESP32 3V3 ---[ 100k ]---+--- GPIO 34
                        |
                  [ gypsum block ]
                        |
ESP32 GND --------------+
```

GPIO 34 is ADC1 channel 6. It is input-only and is not disturbed by WiFi,
unlike the ADC2 pins.

The ADC measures the voltage across the block. A wet block has low resistance
and pulls the node towards ground; a dry block has high resistance and lets the
node rise towards 3.3 V. The firmware converts that voltage back to the block's
resistance in ohms:

```
R_sensor = 100k * V_adc / (3.3 V - V_adc)
```

Resistance is what gets logged and uploaded. Converting resistance to a
moisture percentage needs a calibration curve for your specific block, which
is a later step.

Each reading is flagged with a quality:

| quality | meaning |
|---------|---------|
| `ok`    | within the ADC's characterised range (~150 to 2450 mV) |
| `low`   | below that range: very wet or shorted |
| `high`  | above that range: very dry, resistance is a rough estimate |
| `open`  | node at supply voltage: sensor disconnected. Not uploaded. |

## Prerequisites
- PlatformIO CLI (`pip install platformio`)
- ESP32 dev board (only for flashing; the math can be tested without one)

## Setup

### 1. WiFi credentials

Copy `env.example` to `.env` and fill in your SSID and password. The Makefile
loads `.env` when building and bakes the values into the firmware.

### 2. Configuration

Copy `config.json.example` to `data/config.json` and edit it, then upload it to
the board with `make uploadfs` (only needed when the config changes).

| key | description |
|-----|-------------|
| `wifi_enabled` | Connect to WiFi |
| `wifi_http_enabled` | POST readings to the API |
| `wifi_http_url` | Ingest endpoint, e.g. `http://192.168.1.19:8000/ingest` |
| `ble_enabled` | Advertise readings over Bluetooth |
| `web_server_enabled` | Serve the CSV log over HTTP |
| `data_logging_enabled` | Append readings to the CSV log on flash |
| `system_status_enabled` | Print heap, RSSI and uptime each cycle |
| `sensor_interval_ms` | Time between readings |
| `node_id` | Sensor identity reported to the API |
| `depth_cm` | Burial depth reported to the API |
| `supply_millivolts` | Divider supply, 3300 for the 3V3 pin |
| `series_resistor_ohms` | Value of the series resistor |
| `adc_samples` | Samples per reading; the median is used (max 32) |

## Commands

```bash
make test      # Run host-side unit tests (no board needed)
make build     # Compile the firmware
make upload    # Flash the ESP32
make uploadfs  # Upload data/config.json to the ESP32
make monitor   # Open the serial monitor (115200 baud)
make clean     # Clean build files
make all       # Build, upload and monitor
```

## Developing without a board

- `make test` runs the divider math in `src/soil/soil_math.cpp` natively on
  your machine using PlatformIO's Unity runner. Tests live in
  `test/test_soil_math/`.
- `make build` compiles the full firmware for the ESP32 without needing one
  attached, which catches everything except runtime behaviour.

## Outputs

**HTTP** posts one record per reading, matching the API's ingest schema:

```json
{"node":"soil-1","depth":0,"firmware":"sensor-0.2.0","timestamp":1757289600,"value":83520,"type":"soil_resistance_ohms"}
```

`timestamp` is 0 until NTP has synced; the API fills in server time.

**CSV** on flash at `/sensor_data.csv`:

```
timestamp,adc_raw,millivolts,resistance_ohms,quality
2026-09-08 12:00:00,1874,1502,83520,ok
```

If the header does not match what the firmware expects (for example after
upgrading from the three-channel firmware), the file is recreated on boot.

**BLE** exposes the full reading as JSON on a read/notify characteristic:

```json
{"node":"soil-1","depth":0,"firmware":"sensor-0.2.0","timestamp":"2026-09-08 12:00:00","adc_raw":1874,"millivolts":1502,"resistance_ohms":83520,"quality":"ok"}
```

**Web** at `http://<board-ip>/` lists, downloads and clears the CSV log.

## Layout

```
src/
  main.cpp               wiring and the read/log/upload loop
  config.*               /config.json loader
  version.h              FIRMWARE_VERSION
  soil/soil_math.*       pure divider math (host-testable)
  soil/SoilSensor.*      ADC sampling on the ESP32
  telemetry/Measurement.* one reading and its CSV / JSON forms
  storage/DataLog.*      CSV log on SPIFFS
  net/WifiLink.*         WiFi connect
  net/Uplink.*           HTTP POST
  net/WebPortal.*        HTTP UI for the log
  net/BleLink.*          BLE GATT server
  system/Clock.*         NTP time
  system/SystemStatus.*  heap / RSSI / uptime logging
test/test_soil_math/     host unit tests
```

## Notes

### DC excitation and gypsum blocks

The block is currently driven with a constant DC voltage from the 3V3 pin.
Gypsum blocks polarise under DC, which makes readings drift over hours or
days and slowly degrades the electrodes. The usual fix is to power the
divider from a GPIO instead of 3V3 and only energise it for a few
milliseconds around each reading, or to alternate polarity with two GPIOs.
The `SoilSensor` class is the place to add that.

### Power

Currently everything is always on. See the git history of this README for the
earlier list of deep-sleep and battery ideas; they still apply.
