#include "SystemStatus.h"

#include <Arduino.h>
#include <WiFi.h>

#include "logger.h"

void printSystemStatus() {
    logln("-----> System Status Check...");
    logln("-----> Free heap: " + String(ESP.getFreeHeap()) + " bytes");

    if (WiFi.status() == WL_CONNECTED) {
        logln("-----> WiFi RSSI: " + String(WiFi.RSSI()) + "dBm");
    }

    logln("-----> Uptime: " + String(millis() / 1000) + " seconds");
    logln("-----> CPU Frequency: " + String(ESP.getCpuFreqMHz()) + " MHz");
}
