#include "WifiLink.h"

#include <Arduino.h>
#include <WiFi.h>

#include "logger.h"

bool connectToWiFi(const char* ssid, const char* password) {
    logln("\n-----> Connecting to WiFi...");

    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(1000);

    logln("-----> Scanning for networks...");
    int n = WiFi.scanNetworks();
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
        logln("ERROR: Target network '" + String(ssid) + "' not found!");
        return false;
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

        if (retries % 5 == 0) {
            int status = WiFi.status();
            logln("\n-----> Attempt " + String(retries) + "/30, Status: " + String(status));
            if (status == WL_CONNECT_FAILED) {
                logln("-----> Connection failed, retrying...");
                WiFi.disconnect();
                delay(1000);
                WiFi.begin(ssid, password);
            }
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
