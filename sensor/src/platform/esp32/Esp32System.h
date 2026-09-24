// Esp32System.h
//
// ISystem on the ESP32: identity from the eFuse MAC, boot counter in NVS,
// wall clock from the RTC (which survives deep sleep), uptime from esp_timer.
#pragma once
#include <Preferences.h>

#include "telemetry/system.h"

class Esp32System : public telemetry::ISystem {
public:
    // Increments the boot counter unless this is a wake from deep sleep.
    bool begin();

    void     deviceId(uint8_t out[6]) override;
    uint16_t bootId() override { return boot_id_; }
    uint32_t uptimeSeconds() override;
    uint32_t unixTime() override;
    bool     setUnixTime(uint32_t unix_time) override;
    uint16_t batteryMillivolts() override { return 0; }  // no sense line on the dev board
    const char* firmwareVersion() override;

    // "TLM-" + last three MAC bytes, e.g. TLM-DDEEFF. NUL terminated, 11 chars.
    void bleName(char out[11]);

    // "aabbccddeeff", NUL terminated, 13 chars.
    void deviceIdHex(char out[13]);

private:
    Preferences prefs_;
    uint16_t    boot_id_ = 0;
    uint8_t     mac_[6]  = {0};
};
