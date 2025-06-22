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

// === Sensor configuration ===
enum SensorDepth {
    DEPTH_TOP = 10,
    DEPTH_MID = 20,
    DEPTH_BOT = 30,
};

const int NUM_SENSORS = 3;
const int sensor_pins[NUM_SENSORS] = {34, 35, 36}; // Adjust based on your wiring

// === Structure for sensor data ===
typedef struct {
    int node;
    int depth;
    int value;
} sensor_message_t;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    logln("-----> 📤 Send status: " + String(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Failed"));
}

// === Sensor reading logic ===
float readSensorAtDepth(int adc_pin, SensorDepth depth) {
    int adc_value = analogRead(adc_pin);
    float voltage = adc_value * (3.3 / 4095.0);
    float r_sensor = (3300.0 / voltage - 1) * 10000.0;
    float cb = 0.118 * r_sensor + 0.004 * sqrt(r_sensor);
    float kPa = cb;

    if (adc_value < 1000 || adc_value > 4000) {
    logln("⚠️  Likely floating pin or bad connection.");
    }
    logln("\n-----> ADC[" + String(depth) + "] = " + String(adc_value));
    logln("         V: " + String(voltage) + " V");
    logln("         R: " + String(r_sensor) + " Ω");
    logln("         kPa: " + String(kPa));

    return kPa;
}

void sendSensorData(int node, SensorDepth depth, int value, const uint8_t *gatewayMac) {
    sensor_message_t sensorData = {
        .node = node,
        .depth = depth,
        .value = value
    };

    esp_err_t result = esp_now_send(gatewayMac, (uint8_t*)&sensorData, sizeof(sensor_message_t));
    if (result == ESP_OK) {
        logln("📡 Sent data for depth " + String(depth));
    } else {
        logln("❌ Send failed");
    }
}

// === Setup ===
void setup() {
    Serial.begin(115200);
    analogReadResolution(12);
    delay(1000);

    logln("\n=== Sensor Starting ===");

    initializeWiFiStationMode();
    if (!initializeESPNOW()) return;

    const uint8_t channel = 1;
    configureWiFiChannel(channel);
    registerSendCallback(OnDataSent);

    const uint8_t gatewayMac[] = {0xEC, 0xE3, 0x34, 0xC0, 0x2F, 0xD8};
    if (!addPeer(gatewayMac, channel)) return;

    logln("-----> 🔍 MAC Address: " + WiFi.macAddress());
    logln("-----> ✨ Sensor ready! ✨");
}

// === Main loop ===
void loop() {
    const SensorDepth depths[NUM_SENSORS] = {DEPTH_TOP, DEPTH_MID, DEPTH_BOT};
    const int node_id = 1;

    for (int i = 0; i < NUM_SENSORS; i++) {
        float kPa = readSensorAtDepth(sensor_pins[i], depths[i]);
        sendSensorData(node_id, depths[i], (int)kPa, (const uint8_t[]){0xEC, 0xE3, 0x34, 0xC0, 0x2F, 0xD8});
        delay(500); // Small pause between sends
    }

    delay(10000);  // Main loop delay
    logln("\n=== Sensor loop complete ===\n");
}
