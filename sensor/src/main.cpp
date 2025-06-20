// === System includes ===
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
extern "C" {
  #include "esp_wifi.h"
}

// === Project includes ===
#include "logger.h"

// Structure for sensor data
typedef struct {
    int node;
    int depth;
    int value;
} sensor_message_t;

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    logln("📤 Send status: " + String(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Failed"));
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    logln("\n=== Sensor Starting ===");
    
    // Initialize WiFi in Station mode
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    // Initialize ESP-NOW
    if (esp_now_init() != ESP_OK) {
        logln("❌ ESP-NOW init failed");
        return;
    }
    logln("✅ ESP-NOW initialized");

    // Set channel
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    logln("📻 Set to channel 1");

    // Register callback
    esp_now_register_send_cb(OnDataSent);

    // Add gateway as peer
    esp_now_peer_info_t peerInfo = {};
    // Gateway MAC from serial output
    uint8_t gatewayMac[] = {0xEC, 0xE3, 0x34, 0xC0, 0x2F, 0xD8};  // Your gateway's actual MAC
    memcpy(peerInfo.peer_addr, gatewayMac, 6);
    peerInfo.channel = 1;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        logln("❌ Failed to add peer");
        return;
    }
    logln("✅ Added peer");
    
    logln("🔍 MAC Address: " + WiFi.macAddress());
    logln("✨ Sensor ready!");
}

void loop() {
    // Create test data
    sensor_message_t sensorData = {
        .node = 1,
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