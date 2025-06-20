#include "wifi_manager.h"
#include <WiFi.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <vector>

namespace wifi_manager {

    struct NetworkCredential {
        String ssid;
        String password;
    };

    static bool connected = false;

    void connectToBestNetwork() {
        // Mount SPIFFS
        if (!SPIFFS.begin(true)) {
            Serial.println("[WiFiManager] ❌ Failed to mount SPIFFS");
            return;
        }

        // Open config file
        File file = SPIFFS.open("/wifi.json", "r");
        if (!file) {
            Serial.println("[WiFiManager] ❌ Could not open /wifi.json");
            return;
        }

        // Parse JSON
        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, file);
        if (error) {
            Serial.println("[WiFiManager] ❌ Failed to parse wifi.json");
            return;
        }

        // Load known networks
        std::vector<NetworkCredential> knownNetworks;
        for (JsonObject obj : doc.as<JsonArray>()) {
            knownNetworks.push_back({obj["ssid"].as<String>(), obj["password"].as<String>()});
        }

        WiFi.mode(WIFI_STA);
        WiFi.disconnect(true);
        delay(100);

        int n = WiFi.scanNetworks();
        Serial.println("[WiFiManager] Scanning for networks...");

        int bestSignal = -1000;
        NetworkCredential* bestMatch = nullptr;

        for (int i = 0; i < n; i++) {
            String scannedSSID = WiFi.SSID(i);
            int rssi = WiFi.RSSI(i);

            for (auto& net : knownNetworks) {
                if (scannedSSID == net.ssid && rssi > bestSignal) {
                    bestSignal = rssi;
                    bestMatch = &net;
                }
            }
        }

        if (bestMatch != nullptr) {
            Serial.printf("[WiFiManager] 🔌 Connecting to: %s (%ddBm)\n",
                          bestMatch->ssid.c_str(), bestSignal);

            WiFi.begin(bestMatch->ssid.c_str(), bestMatch->password.c_str());

            int retries = 0;
            while (WiFi.status() != WL_CONNECTED && retries < 15) {
                delay(500);
                Serial.print(".");
                retries++;
            }

            if (WiFi.status() == WL_CONNECTED) {
                connected = true;
                Serial.println("\n✅ Connected! IP: " + WiFi.localIP().toString());
            } else {
                Serial.println("\n❌ Connection failed.");
            }
        } else {
            Serial.println("[WiFiManager] ⚠️ No known networks found.");
        }
    }

    bool isConnected() {
        return connected;
    }

    String getLocalIP() {
        return connected ? WiFi.localIP().toString() : "";
    }

}
