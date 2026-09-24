// Diagnostics.h
//
// Runtime state shared between the main loop and the web portal so the last
// reading, upload result and link health can be inspected from a browser.
#pragma once
#include <Arduino.h>

#include "telemetry/Measurement.h"

struct Config;

struct Diagnostics {
    const Config* config = nullptr;
    bool          bleConnected = false;  // updated by the main loop

    bool          hasReading    = false;
    Measurement   lastReading;
    unsigned long lastReadingMs = 0;

    // 0 = never attempted, <0 = HTTPClient error (no response), else HTTP status.
    int           lastHttpCode   = 0;
    unsigned long lastUploadMs   = 0;
    uint32_t      uploadsOk      = 0;
    uint32_t      uploadsFailed  = 0;
    uint32_t      uploadsSkipped = 0;

    void recordReading(const Measurement& m);
    void recordUpload(int httpCode);
    void recordUploadSkipped();

    // Everything above plus live WiFi, clock, BLE and system info.
    String toJson() const;
};
