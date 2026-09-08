#include <Arduino.h>
#include <WiFi.h>

#include "logger.h"

#include "config.h"
#include "net/BleLink.h"
#include "net/Uplink.h"
#include "net/WebPortal.h"
#include "net/WifiLink.h"
#include "soil/SoilSensor.h"
#include "storage/DataLog.h"
#include "system/Clock.h"
#include "system/SystemStatus.h"
#include "telemetry/Measurement.h"
#include "version.h"

// Gypsum block on the low side of a 100k divider from 3V3, node on GPIO 34
// (ADC1_CH6, input-only, unaffected by WiFi).
#define SOIL_SENSOR_PIN 34

static Config config;
static DataLog dataLog("/sensor_data.csv", Measurement::csvHeader());
static SoilSensor* soilSensor = nullptr;
static unsigned long lastSensorRead = 0;

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

void setup() {
    Serial.begin(115200);
    delay(1000);

    dataLog.begin();
    loadConfig(config);
    printConfig(config);

    soil::DividerConfig divider = {config.supplyMillivolts, config.seriesResistorOhms};
    soilSensor = new SoilSensor(SOIL_SENSOR_PIN, divider, (uint8_t)config.adcSamples);
    soilSensor->begin();

    if (config.bleEnabled) ble_link::begin("ESP32");

    if (config.wifiEnabled && connectToWiFi(WIFI_SSID, WIFI_PASSWORD)) {
        clock_sync::syncNTP();
        if (config.webServerEnabled) web_portal::begin(dataLog);
    }

    logln("\n === SETUP COMPLETE (" FIRMWARE_VERSION ") ===");
}

// The web server needs frequent handle() calls to stay responsive, so it runs
// every tick and the blocking sensor/upload work only runs on its interval.
void loop() {
    if (config.webServerEnabled && WiFi.status() == WL_CONNECTED) {
        web_portal::handle();
    }

    unsigned long now = millis();
    if (now - lastSensorRead >= config.sensorIntervalMs) {
        lastSensorRead = now;
        logln("\n === INNER LOOP ===");

        Measurement m = takeMeasurement();

        if (config.dataLoggingEnabled) {
            dataLog.append(m.toCsv());
        }

        if (config.httpEnabled && WiFi.status() == WL_CONNECTED) {
            if (m.quality == soil::Quality::Open) {
                logln("-----> Skipping upload: sensor reads open circuit");
            } else {
                postJson(config.httpUrl.c_str(), m.toIngestJson().c_str());
            }
        }

        if (config.bleEnabled) {
            ble_link::publish(m.toJson());
        }

        if (config.systemStatusEnabled) {
            printSystemStatus();
        }

        logln("\n === LOOP COMPLETE ===");
    }

    delay(10);
}
