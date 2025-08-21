#pragma once
#include <Arduino.h>
#include <Wire.h>

// Run an I2C bus scan and print results to Serial
// SDA = GPIO3, SCL = GPIO2 (per Waveshare schematic, though seems to be opposite of what the schematic shows)  
void runI2CScanner(TwoWire &wire = Wire, int sda = 3, int scl = 2);