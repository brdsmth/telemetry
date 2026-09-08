#include "SoilSensor.h"

#include "logger.h"

SoilSensor::SoilSensor(uint8_t pin, soil::DividerConfig divider, uint8_t samples)
: pin_(pin), divider_(divider), samples_(samples) {
    if (samples_ < 1) samples_ = 1;
    if (samples_ > kMaxSamples) samples_ = kMaxSamples;
}

void SoilSensor::begin() {
    pinMode(pin_, INPUT);
    analogReadResolution(12);
    // 11 dB attenuation gives the full 0-3.3 V input range needed to see a
    // dry (high resistance) sensor pull the node up towards the supply.
    analogSetPinAttenuation(pin_, ADC_11db);
    logln("-----> Soil sensor on GPIO " + String(pin_) +
          " (" + String(divider_.seriesResistorOhms, 0) + " ohm series, " +
          String(samples_) + " samples)");
}

SoilSensor::Reading SoilSensor::read() {
    float mv[kMaxSamples];
    int raw = 0;

    for (uint8_t i = 0; i < samples_; i++) {
        // analogReadMilliVolts applies the chip's eFuse ADC calibration, so it
        // is closer to the true node voltage than raw * 3300 / 4095.
        mv[i] = (float)analogReadMilliVolts(pin_);
        raw = analogRead(pin_);
        delay(2);
    }

    Reading r;
    r.adcRaw         = raw;
    r.millivolts     = soil::median(mv, samples_);
    r.resistanceOhms = soil::resistanceFromMillivolts(divider_, r.millivolts);
    r.quality        = soil::classify(divider_, r.millivolts);

    logln("-----> Soil: " + String(r.millivolts, 0) + " mV, " +
          (r.resistanceOhms < 0 ? String("open") : String(r.resistanceOhms, 0) + " ohm") +
          " [" + soil::qualityName(r.quality) + "] (raw " + String(r.adcRaw) + ")");
    return r;
}
