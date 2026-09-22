#include "WifiLink.h"

#include <Arduino.h>
#include <WiFi.h>

#include "logger.h"

// Reason codes from esp_wifi_types.h. The ones that matter for debugging:
// 2 / 15 / 202 -> almost always a wrong password, 200 / 201 -> weak or no
// signal, 8 -> the AP told us to leave.
static const char* disconnectReasonName(uint8_t reason) {
    switch (reason) {
        case 2:   return "auth expired";
        case 8:   return "AP sent disassoc";
        case 15:  return "4-way handshake timeout (wrong password?)";
        case 39:  return "timeout";
        case 200: return "beacon timeout (weak signal)";
        case 201: return "no AP found";
        case 202: return "auth failed (wrong password?)";
        case 203: return "assoc failed";
        case 204: return "handshake timeout";
        default:  return "";
    }
}

static void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
        uint8_t reason = info.wifi_sta_disconnected.reason;
        logln("\n-----> WiFi disconnected, reason " + String(reason) + " " +
              disconnectReasonName(reason));
    }
}

bool connectToWiFi(const char* ssid, const char* password) {
    logln("\n-----> Connecting to WiFi...");

    static bool eventHandlerInstalled = false;
    if (!eventHandlerInstalled) {
        WiFi.onEvent(onWiFiEvent);
        eventHandlerInstalled = true;
    }

    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(1000);

    // The scan is only a hint for the log. It is a ~120 ms per channel
    // snapshot and can miss a weak AP that a connection attempt still reaches.
    logln("-----> Scanning for networks...");
    int n = WiFi.scanNetworks(false, /*show_hidden=*/true);
    logln("-----> Found " + String(n) + " networks:");

    bool networkFound = false;
    for (int i = 0; i < n; i++) {
        String scannedSSID = WiFi.SSID(i);
        int rssi = WiFi.RSSI(i);
        logln("  " + String(i) + ": " + scannedSSID + " (" + String(rssi) + "dBm)");
        if (scannedSSID == ssid) {
            networkFound = true;
            logln("  >> Target network found!");
        }
    }

    if (!networkFound) {
        logln("-----> WARNING: '" + String(ssid) + "' not seen in scan (2.4 GHz only), trying anyway");
    }

    logln("-----> Connecting to " + String(ssid) + "...");

    WiFi.setAutoReconnect(false);
    WiFi.persistent(false);
    WiFi.setHostname("ESP32-Sensor");
    WiFi.begin(ssid, password);

    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 30) {
        delay(1000);
        Serial.print(".");
        retries++;

        // Auto-reconnect is off, so after a single failed association the
        // stack sits at WL_DISCONNECTED (6) and never tries again on its own.
        // Kick off a fresh attempt every few seconds whatever the status is.
        if (retries % 5 == 0) {
            int status = WiFi.status();
            logln("\n-----> Attempt " + String(retries) + "/30, Status: " + String(status) + ", retrying...");
            WiFi.disconnect();
            delay(500);
            WiFi.begin(ssid, password);
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        logln("\n-----> WIFI CONNECTED!");
        logln("-----> IP: " + WiFi.localIP().toString());
        logln("-----> RSSI: " + String(WiFi.RSSI()) + "dBm");
        return true;
    }

    logln("\n-----> WIFI CONNECTION FAILED!");
    logln("-----> Final status: " + String(WiFi.status()));
    logln("-----> Status codes: 0=IDLE, 1=NO_SSID, 3=CONNECTED, 4=CONNECT_FAILED, 6=DISCONNECTED");
    return false;
}
