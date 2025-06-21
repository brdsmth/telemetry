// === System includes ===
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
extern "C" {
  #include "esp_wifi.h"
}

// === Project includes ===
#include "logger.h"
#include "espnow_utils.h"

// Structure for sensor data
typedef struct {
    int node;
    int depth;
    int value;
} sensor_message_t;

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    logln("-----> 📤 Send status: " + String(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Failed"));
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    logln("\n=== Sensor Starting ===");

    initializeWiFiStationMode();

    if (!initializeESPNOW()) return;

    const uint8_t channel = 1;
    configureWiFiChannel(channel);
    registerSendCallback(OnDataSent);

    const uint8_t gatewayMac[] = {0xEC, 0xE3, 0x34, 0xC0, 0x2F, 0xD8};  // your gateway
    if (!addPeer(gatewayMac, channel)) return;

    logln("-----> 🔍 MAC Address: " + WiFi.macAddress());
    logln("-----> ✨ Sensor ready! ✨");
}

void loop() {
    // Create test data
    sensor_message_t sensorData = {
        .node = random(1000, 1010),
        .depth = 20,
        .value = random(0, 1000)
    };

    // Send data to gateway MAC
    uint8_t gatewayMac[] = {0xEC, 0xE3, 0x34, 0xC0, 0x2F, 0xD8};
    esp_err_t result = esp_now_send(gatewayMac, (uint8_t*)&sensorData, sizeof(sensor_message_t));
    
    if (result == ESP_OK) {
        logln("📡 Sending data to gateway...");
    } else {
        logln("❌ Send failed");
    }

    delay(5000);  // Wait 5 seconds before next transmission
} 