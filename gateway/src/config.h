// config.h
#pragma once
#include <Arduino.h>

// Serial port mapping for modem
#define MODEM        Serial1
#define MODEM_BAUD   115200
#define RX_PIN       17
#define TX_PIN       18

// API Gateway endpoint
// Declaration only (not definition!)
extern const char* SERVER_URL;