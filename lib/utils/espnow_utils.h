#ifndef ESPNOW_UTIL_H
#define ESPNOW_UTIL_H

#include <Arduino.h>
#include <esp_now.h>

void initializeWiFiStationMode();
bool initializeESPNOW();
void configureWiFiChannel(uint8_t channel);
void registerSendCallback(esp_now_send_cb_t callback);
void registerRecvCallback(esp_now_recv_cb_t callback);
bool addPeer(const uint8_t* mac, uint8_t channel);

#endif