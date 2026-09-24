// fake_system.h
//
// Deterministic ISystem for host tests and the simulator. Time only moves
// when a test advances it.
#pragma once
#include <string.h>

#include "telemetry/system.h"

namespace telemetry {
namespace testing {

class FakeSystem : public ISystem {
public:
    FakeSystem() {
        const uint8_t id[6] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
        memcpy(device_id, id, 6);
    }

    void deviceId(uint8_t out[6]) override { memcpy(out, device_id, 6); }
    uint16_t bootId() override { return boot_id; }
    uint32_t uptimeSeconds() override { return uptime_s; }

    uint32_t unixTime() override {
        if (!time_set) return 0;
        return unix_at_set + (uptime_s - uptime_at_set);
    }

    bool setUnixTime(uint32_t unix_time) override {
        set_time_calls++;
        if (reject_set_time) return false;
        time_set      = true;
        unix_at_set   = unix_time;
        uptime_at_set = uptime_s;
        return true;
    }

    uint16_t batteryMillivolts() override { return battery_mv; }
    const char* firmwareVersion() override { return fw_version; }

    void advance(uint32_t seconds) { uptime_s += seconds; }

    uint8_t  device_id[6];
    uint16_t boot_id        = 1;
    uint32_t uptime_s       = 0;
    bool     time_set       = false;
    uint32_t unix_at_set    = 0;
    uint32_t uptime_at_set  = 0;
    uint16_t battery_mv     = 0;
    const char* fw_version  = "sensor-0.3.0";
    bool     reject_set_time = false;
    uint32_t set_time_calls  = 0;
};

}  // namespace testing
}  // namespace telemetry
