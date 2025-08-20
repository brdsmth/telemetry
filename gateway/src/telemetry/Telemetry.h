// Telemetry.h
#pragma once
#include <ArduinoJson.h>

struct Telemetry {
  String device;
  String status;
  String timestamp;

  String toJson() const;
};