// Measurement.h
//
// One soil reading plus the identity fields the ingest API expects.
#pragma once
#include <Arduino.h>
#include <time.h>

#include "soil/soil_math.h"

struct Measurement {
    // Identity (from config / build)
    String node;
    int    depthCm;
    String firmware;

    // When
    time_t unixTime;    // 0 if the clock is not synced; API fills in server time
    String timestamp;   // human readable, for the CSV

    // What
    int           adcRaw;
    float         millivolts;
    float         resistanceOhms;   // soil::kOpenCircuit if open
    soil::Quality quality;

    static const char* csvHeader();
    String toCsv() const;

    // Body for POST /ingest: {node, depth, firmware, timestamp, value, type}
    String toIngestJson() const;

    // Full detail, for BLE / debugging.
    String toJson() const;
};
