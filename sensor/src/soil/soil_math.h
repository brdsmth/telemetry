// soil_math.h
//
// Pure arithmetic for a resistive soil sensor read through a voltage divider.
// No Arduino dependencies so it can be unit tested on the host (pio test -e native).
//
// Circuit:
//
//   3V3 ---[ R_series ]---+--- ADC pin
//                         |
//                     [ sensor ]
//                         |
//                        GND
//
// The ADC sees the voltage across the sensor:
//   V_adc = V_supply * R_sensor / (R_series + R_sensor)
// so
//   R_sensor = R_series * V_adc / (V_supply - V_adc)
//
// A wet gypsum block has low resistance (V_adc near 0), a dry one has very high
// resistance (V_adc near V_supply).
#pragma once

#include <stddef.h>

namespace soil {

struct DividerConfig {
    float supplyMillivolts;    // e.g. 3300 for the ESP32 3V3 rail
    float seriesResistorOhms;  // e.g. 100000 for a 100k series resistor
};

// Sentinel returned when the divider reads as open (sensor disconnected or
// resistance too high to resolve).
constexpr float kOpenCircuit = -1.0f;

// ESP32 ADC at 11dB attenuation is only characterised between roughly these
// bounds. Readings outside are still returned but flagged.
constexpr float kAdcLinearMinMillivolts = 150.0f;
constexpr float kAdcLinearMaxMillivolts = 2450.0f;

enum class Quality {
    Ok,        // within the ADC's characterised range
    Low,       // below the ADC's usable floor: sensor near short / very wet
    High,      // above the ADC's usable ceiling: sensor near open / very dry
    Open,      // node voltage at or above supply: no sensor path to ground
};

// Sensor resistance in ohms for a measured node voltage. Negative inputs are
// treated as 0 mV. Returns kOpenCircuit if the node voltage reaches the supply.
float resistanceFromMillivolts(const DividerConfig& cfg, float millivolts);

// Inverse of resistanceFromMillivolts, useful for tests and simulation.
float millivoltsFromResistance(const DividerConfig& cfg, float ohms);

Quality classify(const DividerConfig& cfg, float millivolts);

const char* qualityName(Quality q);

// Median of n samples. Reorders the buffer. Returns 0 for n == 0.
float median(float* samples, size_t n);

}  // namespace soil
