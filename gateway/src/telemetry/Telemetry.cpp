// Telemetry.cpp
#include "Telemetry.h"

String Telemetry::toJson() const {
  JsonDocument doc;
  doc["device"] = device;
  doc["status"] = status;
  doc["timestamp"] = timestamp;
  doc["batteryPresent"] = batteryPresent;
  doc["batteryVoltage"] = batteryVoltage;
  doc["batteryPercent"] = batteryPercent;

  String json;
  serializeJson(doc, json);
  return json;
}