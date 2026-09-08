#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>

#include "logger.h"

#include "config.h"
#include "net/BleLink.h"
#include "net/Uplink.h"
#include "net/WebPortal.h"
#include "net/WifiLink.h"
#include "storage/DataLog.h"
#include "system/Clock.h"
#include "system/SystemStatus.h"

// === Sensor pins ===
#define POWER_PIN     23  // Power supply for breakout board
#define SENSOR_1_PIN  32  // Analog input from voltage divider 1 (ADC1_CH4)
#define SENSOR_2_PIN  33  // Analog input from voltage divider 2 (ADC1_CH5)
#define SENSOR_3_PIN  34  // Analog input from voltage divider 3 (ADC1_CH6)

static Config config;
static DataLog dataLog("/sensor_data.csv", "timestamp,sensor_1,sensor_2,sensor_3");
static unsigned long lastSensorRead = 0;

static String readSensors(float& voltage1, float& voltage2, float& voltage3) {
    int sensor1 = analogRead(SENSOR_1_PIN);
    int sensor2 = analogRead(SENSOR_2_PIN);
    int sensor3 = analogRead(SENSOR_3_PIN);

    // ESP32 ADC is 12-bit: 0-4095 maps to 0-3.3V by default
    voltage1 = sensor1 * (3.3 / 4095.0);
    voltage2 = sensor2 * (3.3 / 4095.0);
    voltage3 = sensor3 * (3.3 / 4095.0);

    logln("-----> Sensor 1 (32): " + String(voltage1, 2) + "V (raw: " + String(sensor1) + ")");
    logln("-----> Sensor 2 (33): " + String(voltage2, 2) + "V (raw: " + String(sensor2) + ")");
    logln("-----> Sensor 3 (34): " + String(voltage3, 2) + "V (raw: " + String(sensor3) + ")");

    StaticJsonDocument<256> doc;
    doc["timestamp"] = clock_sync::timestamp();
    doc["sensor_1"] = serialized(String(voltage1, 2));
    doc["sensor_2"] = serialized(String(voltage2, 2));
    doc["sensor_3"] = serialized(String(voltage3, 2));

    String json;
    serializeJson(doc, json);
    logln("-----> JSON payload: " + json);
    return json;
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    pinMode(POWER_PIN, OUTPUT);
    digitalWrite(POWER_PIN, HIGH);
    logln("-----> Power pin " + String(POWER_PIN) + " set to HIGH");

    pinMode(SENSOR_1_PIN, INPUT);
    pinMode(SENSOR_2_PIN, INPUT);
    pinMode(SENSOR_3_PIN, INPUT);
    logln("-----> Sensor pins initialized");

    dataLog.begin();
    loadConfig(config);
    printConfig(config);

    if (config.bleEnabled) ble_link::begin("ESP32");

    if (config.wifiEnabled && connectToWiFi(WIFI_SSID, WIFI_PASSWORD)) {
        clock_sync::syncNTP();
        if (config.webServerEnabled) web_portal::begin(dataLog);
    }

    logln("\n === SETUP COMPLETE ===");
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

        float v1, v2, v3;
        String json = readSensors(v1, v2, v3);

        if (config.dataLoggingEnabled) {
            dataLog.append(clock_sync::timestamp() + "," +
                           String(v1, 3) + "," + String(v2, 3) + "," + String(v3, 3));
        }

        if (config.httpEnabled && WiFi.status() == WL_CONNECTED) {
            postJson(config.httpUrl.c_str(), json.c_str());
        }

        if (config.bleEnabled) {
            ble_link::publish(json);
        }

        if (config.systemStatusEnabled) {
            printSystemStatus();
        }

        logln("\n === LOOP COMPLETE ===");
    }

    delay(10);
}
