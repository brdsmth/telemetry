#include "Diagnostics.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "config.h"
#include "system/Clock.h"
#include "version.h"

void Diagnostics::recordReading(const Measurement& m) {
    lastReading   = m;
    lastReadingMs = millis();
    hasReading    = true;
}

void Diagnostics::recordUpload(int httpCode) {
    lastHttpCode = httpCode;
    lastUploadMs = millis();
    if (httpCode >= 200 && httpCode < 300) {
        uploadsOk++;
    } else {
        uploadsFailed++;
    }
}

void Diagnostics::recordUploadSkipped() {
    uploadsSkipped++;
}

static String describeHttpCode(int code) {
    if (code == 0) return "never attempted";
    if (code < 0) return HTTPClient::errorToString(code);
    if (code >= 200 && code < 300) return "ok";
    return "rejected by server";
}

static long ageSeconds(unsigned long sinceMs) {
    return (long)((millis() - sinceMs) / 1000);
}

String Diagnostics::toJson() const {
    StaticJsonDocument<1536> doc;

    doc["firmware"]  = FIRMWARE_VERSION;
    doc["uptime_s"]  = millis() / 1000;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["cpu_mhz"]   = ESP.getCpuFreqMHz();

    JsonObject wifi = doc.createNestedObject("wifi");
    bool wifiUp = WiFi.status() == WL_CONNECTED;
    wifi["connected"] = wifiUp;
    if (wifiUp) {
        wifi["ssid"]     = WiFi.SSID();
        wifi["ip"]       = WiFi.localIP().toString();
        wifi["hostname"] = WiFi.getHostname();
        wifi["rssi"]     = WiFi.RSSI();
    }

    JsonObject clk = doc.createNestedObject("clock");
    clk["synced"] = clock_sync::isSynced();
    clk["time"]   = clock_sync::timestamp();

    JsonObject ble = doc.createNestedObject("ble");
    ble["enabled"]   = config ? config->bleEnabled : false;
    ble["connected"] = bleConnected;

    if (config) {
        JsonObject cfg = doc.createNestedObject("config");
        cfg["mode"]                 = config->mode;
        cfg["interval_ms"]          = config->sensorIntervalMs;
        cfg["node_id"]              = config->nodeId;
        cfg["depth_cm"]             = config->depthCm;
        cfg["http_enabled"]         = config->httpEnabled;
        cfg["ingest_url"]           = config->httpUrl;
        cfg["data_logging_enabled"] = config->dataLoggingEnabled;
        cfg["supply_millivolts"]    = config->supplyMillivolts;
        cfg["series_resistor_ohms"] = config->seriesResistorOhms;
        cfg["adc_samples"]          = config->adcSamples;
    }

    if (hasReading) {
        JsonObject r = doc.createNestedObject("reading");
        r["age_s"]           = ageSeconds(lastReadingMs);
        r["timestamp"]       = lastReading.timestamp;
        r["adc_raw"]         = lastReading.adcRaw;
        r["millivolts"]      = serialized(String(lastReading.millivolts, 0));
        r["resistance_ohms"] = serialized(String(lastReading.resistanceOhms, 0));
        r["quality"]         = soil::qualityName(lastReading.quality);
    } else {
        doc["reading"] = nullptr;
    }

    JsonObject up = doc.createNestedObject("upload");
    up["last_code"]   = lastHttpCode;
    up["last_result"] = describeHttpCode(lastHttpCode);
    if (lastHttpCode != 0) up["age_s"] = ageSeconds(lastUploadMs);
    up["ok"]      = uploadsOk;
    up["failed"]  = uploadsFailed;
    up["skipped"] = uploadsSkipped;

    String out;
    serializeJson(doc, out);
    return out;
}
