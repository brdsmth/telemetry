// config.h
//
// Runtime configuration loaded from /config.json on SPIFFS (see data/config.json).
#pragma once
#include <Arduino.h>

struct Config {
    bool   wifiEnabled          = true;
    bool   httpEnabled          = true;
    String httpUrl              = "http://192.168.1.19:8000/";
    bool   bleEnabled           = true;
    bool   webServerEnabled     = true;
    bool   dataLoggingEnabled   = true;
    bool   systemStatusEnabled  = true;
    unsigned long sensorIntervalMs = 10000;

    // Identity reported to the ingest API
    String nodeId              = "soil-1";
    int    depthCm             = 0;

    // Voltage divider (see soil/soil_math.h)
    float  supplyMillivolts    = 3300.0f;
    float  seriesResistorOhms  = 100000.0f;
    int    adcSamples          = 16;
};

// Overwrites cfg with values from /config.json. Missing keys keep their defaults.
// Returns false if the file is absent or unparsable (cfg is left untouched).
bool loadConfig(Config& cfg);

void printConfig(const Config& cfg);
