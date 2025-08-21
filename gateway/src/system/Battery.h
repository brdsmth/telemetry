#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library.h"

class Battery {
public:
    Battery(TwoWire &wirePort = Wire);

    bool begin();
    float readVoltage();          // Battery voltage in volts
    float readPercentage();       // Battery percentage (0–100+ % with decimals)
    bool  isBatteryPresent();

private:
    TwoWire* wire;
    SFE_MAX1704X gauge;
    bool initialized;
};