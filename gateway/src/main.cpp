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

// === Globals ===
Preferences preferences;
const char* device_id = "s1";
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
    logln("Hello World from " + String(device_id));
    delay(5000);
} 