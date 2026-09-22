#include "Uplink.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "logger.h"

int postJson(const char* url, const char* payload) {
    if (WiFi.status() != WL_CONNECTED) {
        logln("-----> ERROR: WiFi not connected");
        return HTTPC_ERROR_CONNECTION_REFUSED;
    }

    HTTPClient http;
    http.begin(url);
    http.setTimeout(3000);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("User-Agent", "ESP32-Sensor/1.0");

    logln("-----> Sending POST to: " + String(url));
    logln("-----> Payload: " + String(payload));
    int code = http.POST(payload);

    if (code > 0) {
        String response = http.getString();
        logln("-----> HTTP Response Code: " + String(code));
        logln("-----> Response: " + response);
        http.end();
        return code;
    }

    logln("-----> HTTP Request failed, error: " + String(code) + " (" + HTTPClient::errorToString(code) + ")");
    http.end();
    return code;
}
