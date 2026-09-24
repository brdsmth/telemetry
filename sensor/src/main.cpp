#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>

#include "logger.h"

#include "config.h"
#include "net/Uplink.h"
#include "net/WebPortal.h"
#include "net/WifiLink.h"
#include "platform/esp32/Esp32System.h"
#include "platform/esp32/LittleFsSlotStorage.h"
#include "platform/esp32/NimBleSyncTransport.h"
#include "platform/esp32/NvsCursorStore.h"
#include "soil/SoilSensor.h"
#include "storage/DataLog.h"
#include "system/Clock.h"
#include "system/Diagnostics.h"
#include "system/SystemStatus.h"
#include "telemetry/Measurement.h"
#include "telemetry/record.h"
#include "telemetry/ring_store.h"
#include "telemetry/sync_session.h"
#include "version.h"

// Gypsum block on the low side of a 100k divider from 3V3, node on GPIO 34
// (ADC1_CH6, input-only, unaffected by WiFi).
#define SOIL_SENSOR_PIN 34

static Config config;
static DataLog dataLog("/sensor_data.csv", Measurement::csvHeader());
static SoilSensor* soilSensor = nullptr;
static Diagnostics diagnostics;
static unsigned long lastSensorRead = 0;

// Store-and-forward path: readings become fixed records in a flash ring, and
// the BLE sync service streams them to a phone. See docs/ARCHITECTURE.md.
static Esp32System           sys;
static LittleFsSlotStorage*  slotStorage = nullptr;
static NvsCursorStore*       cursorStore = nullptr;
static telemetry::RingStore* ring        = nullptr;
static telemetry::SyncSession* session   = nullptr;
static NimBleSyncTransport*  ble         = nullptr;

static Measurement takeMeasurement() {
    SoilSensor::Reading r = soilSensor->read();

    Measurement m;
    m.node           = config.nodeId;
    m.depthCm        = config.depthCm;
    m.firmware       = FIRMWARE_VERSION;
    m.unixTime       = clock_sync::unixTime();
    m.timestamp      = clock_sync::timestamp();
    m.adcRaw         = r.adcRaw;
    m.millivolts     = r.millivolts;
    m.resistanceOhms = r.resistanceOhms;
    m.quality        = r.quality;
    return m;
}

// One reading as a wire record. Time is unix seconds when the clock is set
// (NTP on the bench, SET_TIME from a phone in the field), else uptime.
static telemetry::Record toRecord(const Measurement& m) {
    telemetry::Record r;
    r.type    = static_cast<uint8_t>(telemetry::ReadingType::SoilResistanceOhms);
    r.quality = static_cast<uint8_t>(m.quality);  // soil::Quality shares the protocol numbering
    r.value   = m.resistanceOhms;
    r.boot_id = sys.bootId();
    uint32_t now = sys.unixTime();
    if (now != 0) {
        r.flags = telemetry::kFlagEpochValid;
        r.time  = now;
    } else {
        r.time = sys.uptimeSeconds();
    }
    return r;
}

static void beginStore() {
    slotStorage = new LittleFsSlotStorage("/ring.bin", config.ringSlots);
    if (!slotStorage->begin()) {
        logln("-----> ERROR: ring storage unavailable, readings will not be kept for BLE sync");
        return;
    }
    cursorStore = new NvsCursorStore(config.ringSlots);
    cursorStore->begin();
    ring = new telemetry::RingStore(*slotStorage, *cursorStore, telemetry::FullPolicy::DropOldest);
    if (!ring->begin()) {
        logln("-----> Ring cursor reset (first boot or capacity change)");
    }
    const telemetry::RingCursor& c = ring->cursor();
    logln("-----> Ring: " + String(ring->size()) + "/" + String(ring->capacity()) + " records, next seq " +
          String(c.next_seq) + ", collected " + String(c.collected_through) + ", secured " +
          String(c.secured_through) + ", dropped " + String(c.dropped));
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    if (!LittleFS.begin(true)) {
        logln("-----> ERROR: LittleFS mount failed");
    }
    dataLog.begin();
    loadConfig(config);
    printConfig(config);
    diagnostics.config = &config;

    sys.begin();
    beginStore();

    soil::DividerConfig divider = {config.supplyMillivolts, config.seriesResistorOhms};
    soilSensor = new SoilSensor(SOIL_SENSOR_PIN, divider, (uint8_t)config.adcSamples);
    soilSensor->begin();

    // Connect WiFi before starting BLE: both share the radio, and BLE
    // advertising during association costs auth frames on a weak link.
    if (config.wifiEnabled && connectToWiFi(WIFI_SSID, WIFI_PASSWORD)) {
        clock_sync::syncNTP();
        if (config.webServerEnabled) web_portal::begin(dataLog, diagnostics);
    }

    if (config.bleEnabled && ring) {
        session = new telemetry::SyncSession(*ring, sys);
        ble     = new NimBleSyncTransport(*session);
        char name[11];
        sys.bleName(name);
        ble->begin(name);
    }

    logln("\n === SETUP COMPLETE (" FIRMWARE_VERSION ") ===");
}

// The web server and BLE transport need frequent service, so they run every
// tick and the blocking sensor/upload work only runs on its interval.
void loop() {
    if (config.webServerEnabled && WiFi.status() == WL_CONNECTED) {
        web_portal::handle();
    }
    if (ble) {
        ble->loop();
        diagnostics.bleConnected = ble->connected();
    }

    unsigned long now = millis();
    if (now - lastSensorRead >= config.sensorIntervalMs) {
        lastSensorRead = now;
        logln("\n === INNER LOOP ===");

        Measurement m = takeMeasurement();
        diagnostics.recordReading(m);

        if (ring) {
            telemetry::Record r = toRecord(m);
            telemetry::AppendResult res = ring->append(r);
            switch (res) {
                case telemetry::AppendResult::Stored:
                case telemetry::AppendResult::StoredAfterDrop:
                    logln("-----> Ring: stored seq " + String(r.seq) +
                          (res == telemetry::AppendResult::StoredAfterDrop ? " (oldest dropped)" : "") +
                          ", pending " + String(ring->pending()));
                    break;
                case telemetry::AppendResult::Full:
                    logln("-----> Ring: full, reading not stored");
                    break;
                case telemetry::AppendResult::StorageError:
                    logln("-----> ERROR: ring write failed");
                    break;
            }
            if (ble) ble->refreshAdvertising();
        }

        if (config.dataLoggingEnabled) {
            dataLog.append(m.toCsv());
        }

        if (config.httpEnabled && WiFi.status() == WL_CONNECTED) {
            if (m.quality == soil::Quality::Open) {
                logln("-----> Skipping upload: sensor reads open circuit");
                diagnostics.recordUploadSkipped();
            } else {
                int code = postJson(config.httpUrl.c_str(), m.toIngestJson().c_str());
                diagnostics.recordUpload(code);
            }
        }

        if (config.systemStatusEnabled) {
            printSystemStatus();
        }

        logln("\n === LOOP COMPLETE ===");
    }

    delay(10);
}
