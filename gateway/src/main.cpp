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

// Callback when data is received
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    
    logln("📡 Received data from: " + String(macStr));

    if (data_len == sizeof(sensor_message_t)) {
        sensor_message_t* message = (sensor_message_t*)data;
        logln("📦 Data: node=" + String(message->node) + 
              ", depth=" + String(message->depth) + 
              ", value=" + String(message->value));
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    logln("\n=== Gateway Starting ===");
    
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
    esp_now_register_recv_cb(OnDataRecv);
    
    logln("🔍 MAC Address: " + WiFi.macAddress());
    logln("✨ Gateway ready!");
}

void loop() {
    // Just keep the ESP32 running
    delay(10);
} 