#include "Measurement.h"

#include <ArduinoJson.h>

static const char* kType = "soil_resistance_ohms";

const char* Measurement::csvHeader() {
    return "timestamp,adc_raw,millivolts,resistance_ohms,quality";
}

String Measurement::toCsv() const {
    return timestamp + "," +
           String(adcRaw) + "," +
           String(millivolts, 0) + "," +
           String(resistanceOhms, 0) + "," +
           soil::qualityName(quality);
}

String Measurement::toIngestJson() const {
    StaticJsonDocument<256> doc;
    doc["node"]      = node;
    doc["depth"]     = depthCm;
    doc["firmware"]  = firmware;
    doc["timestamp"] = (long long)unixTime;
    doc["value"]     = serialized(String(resistanceOhms, 0));
    doc["type"]      = kType;

    String out;
    serializeJson(doc, out);
    return out;
}

String Measurement::toJson() const {
    StaticJsonDocument<384> doc;
    doc["node"]            = node;
    doc["depth"]           = depthCm;
    doc["firmware"]        = firmware;
    doc["timestamp"]       = timestamp;
    doc["adc_raw"]         = adcRaw;
    doc["millivolts"]      = serialized(String(millivolts, 0));
    doc["resistance_ohms"] = serialized(String(resistanceOhms, 0));
    doc["quality"]         = soil::qualityName(quality);

    String out;
    serializeJson(doc, out);
    return out;
}
