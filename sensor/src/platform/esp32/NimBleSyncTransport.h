// NimBleSyncTransport.h
//
// The BLE side of the sync service (schema/PROTOCOL.md §3) on NimBLE. All
// SyncSession calls happen from loop(); NimBLE callbacks only enqueue.
//
//   Control write  -> queue -> loop(): handleControl -> Status notify
//   loop()         -> nextChunk -> Data notify, one chunk per call
//   Device Info    -> onRead fills the value from the session
//   Advertising    -> refreshAdvertising() after each sample
#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "telemetry/sync_session.h"

class NimBLECharacteristic;
class NimBLEServer;

class NimBleSyncTransport {
public:
    explicit NimBleSyncTransport(telemetry::SyncSession& session);

    // Initialises NimBLE, builds the service and starts advertising.
    bool begin(const char* deviceName);

    // Call every tick. Handles queued commands and streams at most one chunk.
    void loop();

    // Rebuilds the manufacturer data from the session. Takes effect at the
    // next advertising start (immediately when not connected).
    void refreshAdvertising();

    bool connected() const { return connected_; }
    uint16_t mtu() const { return mtu_; }

    // --- called from NimBLE callbacks (host task) ---
    void onConnected(uint16_t mtu);
    void onDisconnected();
    void onPeerMtu(uint16_t mtu);
    void onControlWrite(const uint8_t* data, size_t len);
    void onDeviceInfoRead(NimBLECharacteristic* c);

private:
    struct Pending {
        uint8_t len;
        uint8_t bytes[telemetry::ble::kMaxCommandSize];
    };

    void notifyStatus(const telemetry::ble::Status& s);
    void pumpChunk();
    size_t notifyPayloadLimit() const;

    telemetry::SyncSession& session_;
    NimBLEServer*           server_       = nullptr;
    NimBLECharacteristic*   infoChar_     = nullptr;
    NimBLECharacteristic*   controlChar_  = nullptr;
    NimBLECharacteristic*   dataChar_     = nullptr;
    NimBLECharacteristic*   statusChar_   = nullptr;
    QueueHandle_t           commands_     = nullptr;
    volatile bool           connected_    = false;
    volatile bool           disconnected_ = false;  // edge flag consumed in loop()
    volatile uint16_t       mtu_          = 23;
    char                    name_[16]     = {0};
    unsigned long           lastChunkMs_  = 0;
};
