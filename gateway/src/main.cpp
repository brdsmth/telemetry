// === System includes ===
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <vector>
extern "C" {
  #include "esp_wifi.h"
}

// === Project includes ===
#include "logger.h"
#include "post_client.h"
#include "espnow_utils.h"
#include "wifi_manager.h"

// Structure for sensor data
typedef struct {
    int node;  
    int depth;
    int value;
} sensor_message_t;

// Flushing messages to server
std::vector<String> messageQueue;
unsigned long lastFlushTime = 0;
const unsigned long FLUSH_INTERVAL_MS = 60000;  // every 60s
const size_t MAX_QUEUE_SIZE = 5;


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
              ", value=" + String(message->value) +
              ", timestamp=" + String(time(nullptr)));

        // Convert to JSON
        String json = "{";
        json += "\"node\":\"" + String(message->node) + "\",";  // ✅ now a string
        json += "\"depth\":" + String(message->depth) + ",";
        json += "\"firmware\":\"0.0.1\",";
        json += "\"timestamp\":" + String(time(nullptr)) + ",";
        json += "\"value\":" + String(message->value) + ",";
        json += "\"type\":\"soil\"";
        json += "}";

        messageQueue.push_back(json);
        logln("🗃️ Queued message. Queue size: " + String(messageQueue.size()));
    } else {
        logln("⚠️ Received unexpected payload size");
    }
}


// Flush queue to server 
void flushQueueToServer() {
    if (messageQueue.empty()) return;

    logln("🚪 Flushing " + String(messageQueue.size()) + " messages to server...");

    // Shutdown ESP-NOW
    esp_now_deinit();
    delay(100);

    // Connect to WiFi
    WiFi.mode(WIFI_STA);
    // WiFi.begin("YOUR_WIFI_SSID", "YOUR_WIFI_PASSWORD");
    wifi_manager::connectToBestNetwork();

    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 10) {
        delay(500);
        Serial.print(".");
        retries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        logln("\n🌐 Connected to WiFi");

        for (const auto& msg : messageQueue) {
            post_client::sendJsonPost("http://192.168.0.224:8080/ingest", msg);
        }

        messageQueue.clear();
        logln("✅ Flushed queue");
    } else {
        logln("\n❌ WiFi connection failed — messages retained");
    }

    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(1000); // ensure full shutdown

    // Reinit ESP-NOW
    initializeWiFiStationMode();

    if (!initializeESPNOW()) return;

    const uint8_t channel = 1;
    configureWiFiChannel(channel);
    registerRecvCallback(OnDataRecv);

    logln("📡 ESP-NOW reinitialized");
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    logln("\n=== Gateway Starting ===");
    
    initializeWiFiStationMode();

    if (!initializeESPNOW()) return;

    const uint8_t channel = 1;
    configureWiFiChannel(channel);
    registerRecvCallback(OnDataRecv);
    
    logln("-----> 🔍 MAC Address: " + WiFi.macAddress());
    logln("-----> ✨ Gateway ready!");

    // TODO: Handling time syncing on startup
}

void loop() {
    // Just keep the ESP32 running
    delay(1000);

    unsigned long now = millis();
    if (now - lastFlushTime > FLUSH_INTERVAL_MS) {
        flushQueueToServer();
        lastFlushTime = now;
    }

    if (messageQueue.size() >= MAX_QUEUE_SIZE) {
        logln("⚠️ Queue full. Flushing now...");
        flushQueueToServer();
    } else {
        logln("🗃️ Queue size: " + String(messageQueue.size()));
    }
} 