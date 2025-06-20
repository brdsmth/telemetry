// === System includes ===
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <Preferences.h>

// === Project includes ===
#include "logger.h"
#include "memory.h"
#include "wifi_manager.h"
#include "post_client.h"

// === Configuration ===
#define FIRMWARE_VERSION "0.0.1"
#define SERVER_URL "http://192.168.0.224:8080/ingest"
#define REPORT_INTERVAL_MS 5000

// === Globals ===
Preferences preferences;
const char* device_id = "sensor-001";
int bootCount = 0;

void setup() {
    Serial.begin(115200);
    delay(5000);

    logln("Starting up...");
    logln("\n================================================\n");

    logln("Device ID: " + String(device_id));
    logln("Firmware version: " + String(FIRMWARE_VERSION));

    WiFi.mode(WIFI_STA);
    logln("MAC Address: " + WiFi.macAddress());

    Serial.println();
    
    preferences.begin(device_id, false);
    
    bootCount = preferences.getInt("bootCount", 0);
    bootCount++;
    
    preferences.putInt("bootCount", bootCount);
    
    logkv("Boot count", bootCount);
    printMemoryStats();
    logln("\n================================================\n");

    wifi_manager::connectToBestNetwork();

    if (wifi_manager::isConnected()) {
        Serial.println("IP: " + wifi_manager::getLocalIP());
		String payload = "{\"node\":\"sensor-002\", \"depth\":10, \"timestamp\":0,\"value\":42,\"type\":\"soil_moisture\"}";
        post_client::sendJsonPost(SERVER_URL, payload);
    } else {
        Serial.println("Falling back to SIM...");
    }
}

void loop() {
    logln("Looping... " + String(device_id));
    delay(1000);

    
    // Simulate an incoming sensor reading
    int sensor_id = random(100, 1000);
    int depth = random(1, 11) * 10;
    int value = random(0, 1000);
    String payload = "{\"node\":\"sensor-" + String(sensor_id) + "\", \"depth\":" + String(depth) + ", \"timestamp\":0,\"value\":" + String(value) + ",\"type\":\"soil_moisture\"}";
    
    // Calculate payload size
    int payloadSize = payload.length();
    logln("Payload size: " + String(payloadSize) + " bytes");
    logln("Payload: " + payload);

    if (wifi_manager::isConnected()) {
        post_client::sendJsonPost(SERVER_URL, payload);\
        logln("\n================================================\n");
    } else {
        Serial.println("Falling back to SIM...");
    }

    delay(REPORT_INTERVAL_MS);
} 