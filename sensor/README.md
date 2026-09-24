# ESP32 Soil Sensor

Reads a homemade gypsum block soil moisture sensor through a voltage divider,
stores every reading in a flash ring, and serves the stored readings to a
phone over the BLE sync service. On the bench it also logs a CSV and posts
each reading to the API over WiFi.

The store-and-forward design is in [`docs/ARCHITECTURE.md`](../docs/ARCHITECTURE.md);
the byte formats are in [`schema/PROTOCOL.md`](../schema/PROTOCOL.md).

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

Resistance is what gets stored and uploaded. Converting resistance to a
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
- ESP32 dev board (only for flashing; the core can be tested without one)

## Setup

### 1. WiFi credentials (bench only)

Copy `env.example` to `.env` and fill in your SSID and password. The Makefile
loads `.env` when building and bakes the values into the firmware.

### 2. Configuration

Copy `config.json.example` to `data/config.json` and edit it, then upload it to
the board with `make uploadfs` (only needed when the config changes; this
rewrites the whole LittleFS image, including the CSV log and the ring).

| key | description |
|-----|-------------|
| `wifi_enabled` | Connect to WiFi |
| `wifi_http_enabled` | POST readings to the API |
| `wifi_http_url` | Ingest endpoint, e.g. `https://api-production-2e52.up.railway.app/ingest` |
| `ble_enabled` | Run the BLE sync service |
| `web_server_enabled` | Serve the diagnostics page over HTTP |
| `data_logging_enabled` | Append readings to the CSV log on flash |
| `system_status_enabled` | Print heap, RSSI and uptime each cycle |
| `mode` | `live` reads every 60 s, `development` every 10 s |
| `sensor_interval_ms` | Optional. Overrides the mode's interval |
| `node_id` | Legacy identity for the bench POST. BLE uses the MAC |
| `depth_cm` | Burial depth reported to the API |
| `supply_millivolts` | Divider supply, 3300 for the 3V3 pin |
| `series_resistor_ohms` | Value of the series resistor |
| `adc_samples` | Samples per reading; the median is used (max 32) |
| `ring_slots` | Records kept on flash, 20 bytes each. 8192 is 160 KB, ~5.7 days at 60 s |

## Commands

```bash
make test      # Run host-side unit tests (no board needed)
make build     # Compile the firmware
make upload    # Flash the ESP32
make uploadfs  # Upload data/config.json to the ESP32
make monitor   # Open the serial monitor (115200 baud)
make clean     # Clean build files
make erase     # Erase the whole flash (also wipes the ring and NVS cursor)
make all       # Build, upload and monitor
```

## Developing without a board

- `make test` runs the `native` environment: the divider math plus everything
  in `lib/telemetry-core` (record codec, ring store, BLE codec, sync session)
  against the golden vectors in `schema/vectors`. Tests live in `test/`.
- `make build` compiles the full firmware for the ESP32 without needing one
  attached, which catches everything except runtime behaviour.

## Exercising the board from a laptop

```bash
PY=/opt/homebrew/Cellar/platformio/*/libexec/bin/python   # has pyserial and bleak
$PY ../scripts/hil/serial_capture.py --seconds 90         # reset and watch the boot
$PY ../scripts/hil/ble_probe.py scan                      # who is advertising
$PY ../scripts/hil/ble_probe.py pull --ack-collected      # pull records like the app will
```

## Outputs

**Flash ring** at `/ring.bin`: fixed 20-byte records, see the protocol doc.
The cursor (next seq, oldest kept, collected and secured watermarks, dropped
count) lives in NVS and survives reboots. Records are only freed when a phone
reports them secured on the server.

**BLE** advertises as `TLM-XXXXXX` (last three MAC bytes) with the telemetry
service UUID and a manufacturer data field holding the pending record count,
battery and flags. Connected phones read Device Info, open a session, set the
clock, pull records from a sequence number as CRC-checked chunks, and
acknowledge. Details in `schema/PROTOCOL.md` §3.

**HTTP** (bench) posts one record per reading, matching the API's legacy
ingest schema:

```json
{"node":"soil-1","depth":0,"firmware":"sensor-0.3.0","timestamp":1757289600,"value":83520,"type":"soil_resistance_ohms"}
```

`timestamp` is 0 until NTP has synced; the API fills in server time.

**CSV** (bench) on flash at `/sensor_data.csv`:

```
timestamp,adc_raw,millivolts,resistance_ohms,quality
2026-09-08 12:00:00,1874,1502,83520,ok
```

If the header does not match what the firmware expects, the file is recreated
on boot.

**Web** (bench) at `http://esp32-sensor.local/` (or `http://<board-ip>/`) is a
diagnostics page with the last reading, upload results, WiFi and BLE state,
and the loaded config. JSON at `/status`; CSV at `/view`, `/download`,
`/clear`.

## Layout

```
src/
  main.cpp                    wiring and the read/store/upload loop
  config.*                    /config.json loader
  version.h                   FIRMWARE_VERSION
  soil/soil_math.*            pure divider math (host-testable)
  soil/SoilSensor.*           ADC sampling on the ESP32
  telemetry/Measurement.*     one reading and its CSV / JSON forms (bench)
  storage/DataLog.*           CSV log on LittleFS (bench)
  platform/esp32/
    LittleFsSlotStorage.*     ring slots in a file
    NvsCursorStore.*          ring cursor in NVS
    Esp32System.*             MAC id, boot counter, RTC clock
    NimBleSyncTransport.*     BLE sync service on NimBLE
  net/WifiLink.*              WiFi connect (bench)
  net/Uplink.*                HTTP POST (bench)
  net/WebPortal.*             HTTP diagnostics (bench)
  system/Clock.*              NTP time (bench)
  system/Diagnostics.*        state for the web portal
  system/SystemStatus.*       heap / RSSI / uptime logging
test/                         host unit tests, one directory per subject
../lib/telemetry-core/        the hardware independent core
```

## Notes

### Time in the field

Without WiFi the board has no clock until a phone connects and sends
`SET_TIME`. Until then records carry seconds since boot and the boot id, and
the phone back-fills wall-clock time for records from the boot it connected
in. The RTC keeps time through deep sleep, so one visit sets it until the next
power loss.

### DC excitation and gypsum blocks

The block is currently driven with a constant DC voltage from the 3V3 pin.
Gypsum blocks polarise under DC, which makes readings drift over hours or
days and slowly degrades the electrodes. The usual fix is to power the
divider from a GPIO instead of 3V3 and only energise it for a few
milliseconds around each reading, or to alternate polarity with two GPIOs.
The `SoilSensor` class is the place to add that.

### Power

Everything is still always on. The field build with deep sleep and an
advertising duty cycle is the next step; see the roadmap in the architecture
document.
