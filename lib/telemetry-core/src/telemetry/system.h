// system.h
//
// What the sync layer needs from the device that is not storage: identity,
// clocks, battery. The ESP32 implements this over eFuse MAC, RTC and NVS;
// tests and the simulator use telemetry/testing/fake_system.h.
#pragma once
#include <stdint.h>

namespace telemetry {

class ISystem {
public:
    virtual ~ISystem() {}

    // Factory MAC, 6 bytes. Stable for the life of the device.
    virtual void deviceId(uint8_t out[6]) = 0;

    // Incremented on every cold boot and persisted.
    virtual uint16_t bootId() = 0;

    virtual uint32_t uptimeSeconds() = 0;

    // Unix seconds, or 0 when the clock has never been set.
    virtual uint32_t unixTime() = 0;

    // Sets the clock and persists whatever is needed so unixTime() survives
    // deep sleep. Returns false if the value was rejected.
    virtual bool setUnixTime(uint32_t unix_time) = 0;

    // 0 when unknown.
    virtual uint16_t batteryMillivolts() = 0;

    // NUL terminated, at most 15 characters are used.
    virtual const char* firmwareVersion() = 0;
};

}  // namespace telemetry
