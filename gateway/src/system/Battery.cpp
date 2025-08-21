#include "Battery.h"

Battery::Battery(TwoWire &wirePort)
: wire(&wirePort), initialized(false) {}

bool Battery::begin() {
    // Waveshare ESP32-S3-SIM7607G: SDA=GPIO3, SCL=GPIO2
    wire->begin(3, 2);

    if (!gauge.begin(*wire)) {
        initialized = false;
        return false;
    }
    initialized = true;
    return true;
}

float Battery::readVoltage() {
    if (!initialized) return -1.0f;

    float v = gauge.getVoltage();
    if (v < 0.5f) return -1.0f;   // Treat as "no battery"
    return v;
}

float Battery::readPercentage() {
    if (!initialized) return -1.0f;

    // --- Direct raw read from MAX17048 SOC register ---
    wire->beginTransmission(0x36);
    wire->write(0x04); // SOC register
    if (wire->endTransmission(false) != 0) return -1.0f;

    if (wire->requestFrom(0x36, 2) != 2) return -1.0f;

    uint8_t msb = wire->read(); // integer part
    uint8_t lsb = wire->read(); // fractional part (1/256 %)

    float soc = msb + (lsb / 256.0f);

    if (soc < 0.0f || soc > 120.0f) {
        return -1.0f; // out-of-range sanity check
    }

    // MAX17048 can overshoot ~110% before calibration, so cap at 100%
    if (soc > 100.0f) soc = 100.0f;

    return soc; // float with decimals
}

bool Battery::isBatteryPresent() {
    float v = readVoltage();
    return v > 0.5f;
}
