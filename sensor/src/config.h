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
};

// Overwrites cfg with values from /config.json. Missing keys keep their defaults.
// Returns false if the file is absent or unparsable (cfg is left untouched).
bool loadConfig(Config& cfg);

void printConfig(const Config& cfg);
