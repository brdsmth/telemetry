#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include "logger.h"
#include "espnow_utils.h"

void initializeWiFiStationMode() {
    logln("-----> Initializing WiFi in Station mode");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
}

bool initializeESPNOW() {
    logln("-----> Initializing ESP-NOW");
    if (esp_now_init() != ESP_OK) {
        logln("❌ ESP-NOW init failed");
        return false;
    }
    logln("-----> ✅ ESP-NOW initialized");
    return true;
}

void configureWiFiChannel(uint8_t channel) {
    logln("-----> Configuring WiFi channel to " + String(channel));
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    logln("-----> ✅ WiFi channel configured");
}

void registerSendCallback(esp_now_send_cb_t callback) {
    logln("-----> Registering send callback");
    esp_now_register_send_cb(callback);
    logln("-----> ✅ Send callback registered");
}

void registerRecvCallback(esp_now_recv_cb_t callback) {
    logln("-----> Registering recv callback");
    esp_now_register_recv_cb(callback);
    logln("-----> ✅ Recv callback registered");
}

bool addPeer(const uint8_t* mac, uint8_t channel) {
    logln("-----> Adding peer with MAC " + String(mac[0], HEX) + ":" + String(mac[1], HEX) + ":" + String(mac[2], HEX) + ":" + String(mac[3], HEX) + ":" + String(mac[4], HEX) + ":" + String(mac[5], HEX));
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, mac, 6);
    peerInfo.channel = channel;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        logln("-----> ❌ Failed to add peer");
        return false;
    }

    logln("-----> ✅ Added peer");
    return true;
}
