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
    DEPTH_BOT = 30,
};

const int NUM_SENSORS = 1;
const int sensor_pins[NUM_SENSORS] = {34}; // Adjust based on your wiring

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
    float r_sensor = (3.3 / voltage - 1) * 10000.0;
    float cb = 0.118 * r_sensor + 0.004 * sqrt(r_sensor);
    float kPa = cb;
    Serial.printf("ADC [%d]: %d | V: %.2f | Ω: %.2f | kPa: %.2f\n", adc_pin, adc_value, voltage, r_sensor, kPa);

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

// 📉 Interpreting kPa Values
// Wet sensor = lower resistance
// That pulls more current through the voltage divider
// Which lowers the voltage at the analog pin
// Lower voltage = lower ADC reading

// 0–10     Saturated	    Soil is soggy, just rained, no stress on plants
// 10–30	Ideal Moisture	Moist but not soaked, excellent for growth
// 40–70	Moderately Dry	Starting to get dry for many crops
// 70–200	Very Dry	    Roots stressed, irrigation needed


// Optimal Wiring
// 3.3V ──> Fixed 10kΩ Resistor ──> ADC Pin ──> Watermark Sensor ──> GND

void loop() {
    const SensorDepth depths[NUM_SENSORS] = {DEPTH_BOT};
    const int node_id = 1;

    for (int i = 0; i < NUM_SENSORS; i++) {
        float kPa = readSensorAtDepth(sensor_pins[i], depths[i]);
        // sendSensorData(node_id, depths[i], (int)kPa, (const uint8_t[]){0xEC, 0xE3, 0x34, 0xC0, 0x2F, 0xD8});
        delay(500); // Small pause between sends
    }

    delay(1000);  // Main loop delay
    logln("\n=== Sensor loop complete ===\n");
}
