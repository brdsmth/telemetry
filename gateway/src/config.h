// config.h
#pragma once
#include <Arduino.h>

// Serial port mapping for modem (Waveshare ESP32-S3-SIM7670G board)
// Note: Verify these pins match your specific board revision
#define MODEM        Serial1
#define MODEM_BAUD   115200
#define RX_PIN       17  // ESP32-S3 RX (connects to SIM7670G TX)
#define TX_PIN       18  // ESP32-S3 TX (connects to SIM7670G RX)

// API Gateway endpoint
// Declaration only (not definition!)
extern const char* SERVER_URL;