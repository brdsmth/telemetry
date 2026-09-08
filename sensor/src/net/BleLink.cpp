#include "BleLink.h"

#include <NimBLEDevice.h>

#include "logger.h"

namespace ble_link {

static const char* kServiceUuid  = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
static const char* kReadCharUuid = "beb5483e-36e1-4688-b7f5-ea07361b26a8";  // sensor -> mobile (READ/NOTIFY)
static const char* kWriteCharUuid = "beb5483f-36e1-4688-b7f5-ea07361b26a9"; // mobile -> sensor (WRITE)

static NimBLECharacteristic* readChar = nullptr;
static NimBLECharacteristic* writeChar = nullptr;
static bool connected = false;

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer*) override {
        connected = true;
        logln("-----> BLE: Device connected");
    }
    void onDisconnect(NimBLEServer*) override {
        connected = false;
        logln("-----> BLE: Device disconnected");
    }
};

class WriteCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* c) override {
        std::string value = c->getValue();
        if (!value.empty()) {
            logln("-----> BLE: Received data: " + String(value.c_str()));
        }
    }
};

void begin(const char* deviceName) {
    NimBLEDevice::init(deviceName);
    NimBLEDevice::setMTU(512);

    NimBLEServer* server = NimBLEDevice::createServer();
    server->setCallbacks(new ServerCallbacks());
    NimBLEService* service = server->createService(kServiceUuid);

    readChar = service->createCharacteristic(kReadCharUuid,
                                             NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    String initial = "{\"status\":\"ready\"}";
    readChar->setValue((uint8_t*)initial.c_str(), initial.length());

    writeChar = service->createCharacteristic(kWriteCharUuid, NIMBLE_PROPERTY::WRITE);
    writeChar->setCallbacks(new WriteCallbacks());

    service->start();
    NimBLEDevice::getAdvertising()->addServiceUUID(kServiceUuid);
    NimBLEDevice::startAdvertising();
    logln("-----> BLE: Advertising as '" + String(deviceName) + "'");
}

void publish(const String& json) {
    if (!readChar) {
        logln("-----> BLE: ERROR - not initialised");
        return;
    }

    readChar->setValue((uint8_t*)json.c_str(), json.length());
    if (connected) {
        readChar->notify();
        logln("-----> BLE: Notification sent");
    } else {
        logln("-----> BLE: Value updated, no device connected");
    }
}

bool isConnected() {
    return connected;
}

}  // namespace ble_link
