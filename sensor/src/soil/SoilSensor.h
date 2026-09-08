// SoilSensor.h
//
// Reads a resistive (gypsum block) soil moisture sensor wired as the low side
// of a voltage divider on an ADC1 pin. See soil_math.h for the circuit.
#pragma once
#include <Arduino.h>

#include "soil/soil_math.h"

class SoilSensor {
public:
    struct Reading {
        int           adcRaw;          // last raw 12-bit sample, for reference
        float         millivolts;      // median of calibrated samples
        float         resistanceOhms;  // soil::kOpenCircuit if open
        soil::Quality quality;
    };

    static constexpr uint8_t kMaxSamples = 32;

    // samples is clamped to [1, kMaxSamples].
    SoilSensor(uint8_t pin, soil::DividerConfig divider, uint8_t samples);

    void begin();
    Reading read();

    uint8_t pin() const { return pin_; }
    const soil::DividerConfig& divider() const { return divider_; }

private:
    uint8_t             pin_;
    soil::DividerConfig divider_;
    uint8_t             samples_;
};
