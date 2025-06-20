// === System includes ===
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <Preferences.h>

// === Project includes ===
#include "logger.h"
#include "memory.h"

// === Configuration ===
#define FIRMWARE_VERSION "0.0.1"

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
}

void loop() {
    logln("Hello World from " + String(device_id));
    delay(5000);
} 