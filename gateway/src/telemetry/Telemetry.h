// Telemetry.h
#pragma once
#include <ArduinoJson.h>

struct Telemetry {
  String device;
  String status;
  String timestamp;
  bool   batteryPresent;    // true if gauge detects a battery
  float  batteryVoltage;    // volts (0.0 if not present)
  float  batteryPercent;    // % (0.0 if not present)

  String toJson() const;
};