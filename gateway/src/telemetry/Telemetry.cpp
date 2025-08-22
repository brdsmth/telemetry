// Telemetry.cpp
#include "Telemetry.h"

String Telemetry::toJson() const {
  StaticJsonDocument<256> doc;

  doc["device"]         = device;
  doc["status"]         = status;
  doc["timestamp"]      = timestamp;
  doc["batteryPresent"] = batteryPresent;
  doc["batteryVoltage"] = batteryVoltage;
  doc["batteryPercent"] = batteryPercent;

  if (latitude != 0.0f || longitude != 0.0f) {
    doc["latitude"]  = latitude;
    doc["longitude"] = longitude;
  } else {
    doc["latitude"]  = nullptr;
    doc["longitude"] = nullptr;
  }

  String json;
  serializeJson(doc, json);
  return json;
}