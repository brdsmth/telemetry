#include "soil_math.h"

#include <algorithm>

namespace soil {

float resistanceFromMillivolts(const DividerConfig& cfg, float millivolts) {
    if (millivolts <= 0.0f) return 0.0f;
    if (millivolts >= cfg.supplyMillivolts) return kOpenCircuit;
    return cfg.seriesResistorOhms * millivolts / (cfg.supplyMillivolts - millivolts);
}

float millivoltsFromResistance(const DividerConfig& cfg, float ohms) {
    if (ohms <= 0.0f) return 0.0f;
    return cfg.supplyMillivolts * ohms / (cfg.seriesResistorOhms + ohms);
}

Quality classify(const DividerConfig& cfg, float millivolts) {
    if (millivolts >= cfg.supplyMillivolts) return Quality::Open;
    if (millivolts > kAdcLinearMaxMillivolts) return Quality::High;
    if (millivolts < kAdcLinearMinMillivolts) return Quality::Low;
    return Quality::Ok;
}

const char* qualityName(Quality q) {
    switch (q) {
        case Quality::Ok:   return "ok";
        case Quality::Low:  return "low";
        case Quality::High: return "high";
        case Quality::Open: return "open";
    }
    return "unknown";
}

float median(float* samples, size_t n) {
    if (n == 0) return 0.0f;
    std::sort(samples, samples + n);
    if (n % 2 == 1) return samples[n / 2];
    return (samples[n / 2 - 1] + samples[n / 2]) / 2.0f;
}

}  // namespace soil
