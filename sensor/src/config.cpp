#include "config.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

#include "logger.h"

static const char* kConfigPath = "/config.json";

static unsigned long intervalForMode(const String& mode) {
    if (mode == "development") return kDevelopmentIntervalMs;
    if (mode != "live") {
        logln("-----> WARNING: unknown mode '" + mode + "', using live interval");
    }
    return kLiveIntervalMs;
}

bool loadConfig(Config& cfg) {
    logln("-----> Loading configuration from LittleFS...");

    if (!LittleFS.exists(kConfigPath)) {
        logln("-----> WARNING: config.json not found, using defaults");
        return false;
    }

    File file = LittleFS.open(kConfigPath, FILE_READ);
    if (!file) {
        logln("-----> ERROR: Could not open config.json");
        return false;
    }

    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        logln("-----> ERROR: Failed to parse config.json: " + String(error.c_str()));
        return false;
    }

    cfg.wifiEnabled         = doc["wifi_enabled"]          | cfg.wifiEnabled;
    cfg.httpEnabled         = doc["wifi_http_enabled"]     | cfg.httpEnabled;
    cfg.httpUrl             = doc["wifi_http_url"]         | cfg.httpUrl;
    cfg.bleEnabled          = doc["ble_enabled"]           | cfg.bleEnabled;
    cfg.webServerEnabled    = doc["web_server_enabled"]    | cfg.webServerEnabled;
    cfg.dataLoggingEnabled  = doc["data_logging_enabled"]  | cfg.dataLoggingEnabled;
    cfg.systemStatusEnabled = doc["system_status_enabled"] | cfg.systemStatusEnabled;
    cfg.mode                = doc["mode"]                  | cfg.mode;
    cfg.sensorIntervalMs    = doc["sensor_interval_ms"]    | intervalForMode(cfg.mode);
    cfg.nodeId              = doc["node_id"]               | cfg.nodeId;
    cfg.depthCm             = doc["depth_cm"]              | cfg.depthCm;
    cfg.supplyMillivolts    = doc["supply_millivolts"]     | cfg.supplyMillivolts;
    cfg.seriesResistorOhms  = doc["series_resistor_ohms"]  | cfg.seriesResistorOhms;
    cfg.adcSamples          = doc["adc_samples"]           | cfg.adcSamples;
    cfg.ringSlots           = doc["ring_slots"]            | cfg.ringSlots;

    logln("-----> Configuration loaded successfully");
    return true;
}

static String yesNo(bool v) { return v ? "YES" : "NO"; }

void printConfig(const Config& cfg) {
    logln("\n=== CONFIGURATION ===");
    logln("-----> WiFi Enabled: " + yesNo(cfg.wifiEnabled));
    logln("-----> WiFi HTTP Enabled: " + yesNo(cfg.httpEnabled));
    logln("-----> WiFi HTTP URL: " + cfg.httpUrl);
    logln("-----> BLE Enabled: " + yesNo(cfg.bleEnabled));
    logln("-----> Web Server Enabled: " + yesNo(cfg.webServerEnabled));
    logln("-----> Data Logging Enabled: " + yesNo(cfg.dataLoggingEnabled));
    logln("-----> System Status Enabled: " + yesNo(cfg.systemStatusEnabled));
    logln("-----> Mode: " + cfg.mode);
    logln("-----> Sensor Interval: " + String(cfg.sensorIntervalMs) + "ms");
    logln("-----> Node ID: " + cfg.nodeId);
    logln("-----> Depth: " + String(cfg.depthCm) + "cm");
    logln("-----> Supply: " + String(cfg.supplyMillivolts, 0) + "mV");
    logln("-----> Series Resistor: " + String(cfg.seriesResistorOhms, 0) + " ohm");
    logln("-----> ADC Samples: " + String(cfg.adcSamples));
    logln("-----> Ring Slots: " + String(cfg.ringSlots));
    logln("=== END CONFIGURATION ===\n");
}
