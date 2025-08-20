// Telemetry.cpp
#include "Telemetry.h"

String Telemetry::toJson() const {
  JsonDocument doc;
  doc["device"] = device;
  doc["status"] = status;
  doc["timestamp"] = timestamp;

  String json;
  serializeJson(doc, json);
  return json;
}