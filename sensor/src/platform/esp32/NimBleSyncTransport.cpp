#include "platform/esp32/NimBleSyncTransport.h"

#include <NimBLEDevice.h>
#include <string.h>

#include <string>

#include "logger.h"

using namespace telemetry;

namespace {

// Minimum spacing between Data notifications. NimBLE drops notifications the
// controller cannot buffer, and the phone recovers by re-requesting from the
// last seq it saw, so this is a throughput knob rather than a correctness one.
constexpr unsigned long kChunkSpacingMs = 15;

NimBleSyncTransport* g_transport = nullptr;

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* server, ble_gap_conn_desc* desc) override {
        if (g_transport) g_transport->onConnected(server->getPeerMTU(desc->conn_handle));
    }
    void onDisconnect(NimBLEServer*) override {
        if (g_transport) g_transport->onDisconnected();
    }
    void onMTUChange(uint16_t mtu, ble_gap_conn_desc*) override {
        if (g_transport) g_transport->onPeerMtu(mtu);
    }
};

class ControlCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* c) override {
        std::string v = c->getValue();
        if (g_transport) g_transport->onControlWrite(reinterpret_cast<const uint8_t*>(v.data()), v.size());
    }
};

class InfoCallbacks : public NimBLECharacteristicCallbacks {
    void onRead(NimBLECharacteristic* c) override {
        if (g_transport) g_transport->onDeviceInfoRead(c);
    }
};

}  // namespace

NimBleSyncTransport::NimBleSyncTransport(SyncSession& session) : session_(session) {}

bool NimBleSyncTransport::begin(const char* deviceName) {
    g_transport = this;
    strncpy(name_, deviceName, sizeof name_ - 1);
    commands_ = xQueueCreate(4, sizeof(Pending));

    NimBLEDevice::init(deviceName);
    NimBLEDevice::setMTU(ble::kMtu);

    server_ = NimBLEDevice::createServer();
    server_->setCallbacks(new ServerCallbacks());
    server_->advertiseOnDisconnect(false);  // we restart with fresh manufacturer data

    NimBLEService* service = server_->createService(ble::kServiceUuid);

    infoChar_ = service->createCharacteristic(ble::kDeviceInfoCharUuid, NIMBLE_PROPERTY::READ);
    infoChar_->setCallbacks(new InfoCallbacks());

    controlChar_ = service->createCharacteristic(ble::kControlCharUuid, NIMBLE_PROPERTY::WRITE);
    controlChar_->setCallbacks(new ControlCallbacks());

    dataChar_   = service->createCharacteristic(ble::kDataCharUuid, NIMBLE_PROPERTY::NOTIFY);
    statusChar_ = service->createCharacteristic(ble::kStatusCharUuid,
                                                NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

    uint8_t initial[ble::kStatusSize] = {0};
    statusChar_->setValue(initial, sizeof initial);

    service->start();

    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    NimBLEAdvertisementData scan;
    scan.setName(deviceName);
    adv->setScanResponseData(scan);

    refreshAdvertising();
    logln("-----> BLE: advertising as '" + String(deviceName) + "' with service " + String(ble::kServiceUuid));
    return true;
}

void NimBleSyncTransport::refreshAdvertising() {
    uint8_t payload[ble::kAdvertisingSize];
    session_.encodeAdvertising(payload);

    // Company id (little-endian) followed by the 6-byte payload.
    std::string mfg;
    mfg.push_back(static_cast<char>(ble::kManufacturerId & 0xFF));
    mfg.push_back(static_cast<char>(ble::kManufacturerId >> 8));
    mfg.append(reinterpret_cast<const char*>(payload), sizeof payload);

    NimBLEAdvertisementData data;
    data.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
    data.setCompleteServices(NimBLEUUID(ble::kServiceUuid));
    data.setManufacturerData(mfg);

    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    bool wasAdvertising = adv->isAdvertising();
    if (wasAdvertising) adv->stop();
    adv->setAdvertisementData(data);
    if (!connected_) adv->start();
}

// --- NimBLE callbacks (host task) ---

void NimBleSyncTransport::onConnected(uint16_t mtu) {
    connected_ = true;
    mtu_       = mtu;
    logln("-----> BLE: central connected, MTU " + String(mtu));
}

void NimBleSyncTransport::onDisconnected() {
    connected_    = false;
    disconnected_ = true;
    logln("-----> BLE: central disconnected");
}

void NimBleSyncTransport::onPeerMtu(uint16_t mtu) {
    mtu_ = mtu;
}

void NimBleSyncTransport::onControlWrite(const uint8_t* data, size_t len) {
    Pending p;
    p.len = static_cast<uint8_t>(len > sizeof p.bytes ? sizeof p.bytes + 1 : len);
    if (len <= sizeof p.bytes) memcpy(p.bytes, data, len);
    if (xQueueSend(commands_, &p, 0) != pdTRUE) {
        logln("-----> BLE: command queue full, dropping write");
    }
}

void NimBleSyncTransport::onDeviceInfoRead(NimBLECharacteristic* c) {
    uint8_t buf[ble::kDeviceInfoSize];
    session_.encodeDeviceInfo(buf);
    c->setValue(buf, sizeof buf);
}

// --- loop task ---

void NimBleSyncTransport::loop() {
    if (disconnected_) {
        disconnected_ = false;
        session_.onDisconnect();
        refreshAdvertising();
    }

    Pending p;
    while (xQueueReceive(commands_, &p, 0) == pdTRUE) {
        ble::Status s;
        // An oversize write arrives with len > kMaxCommandSize and no bytes;
        // hand the session a zero-length command so it answers bad_argument.
        size_t len = p.len > sizeof p.bytes ? 0 : p.len;
        session_.handleControl(p.bytes, len, s);
        notifyStatus(s);
        logln("-----> BLE: cmd 0x" + String(s.opcode, HEX) + " -> " +
              String(ble::resultName(static_cast<ble::Result>(s.result))) + " seq " + String(s.seq));
    }

    if (connected_ && session_.streaming()) pumpChunk();
}

void NimBleSyncTransport::notifyStatus(const ble::Status& s) {
    uint8_t buf[ble::kStatusSize];
    ble::encodeStatus(s, buf);
    statusChar_->setValue(buf, sizeof buf);
    statusChar_->notify();
}

size_t NimBleSyncTransport::notifyPayloadLimit() const {
    // ATT notification payload is MTU minus the 3-byte header.
    uint16_t m = mtu_ < 23 ? 23 : mtu_;
    return static_cast<size_t>(m - 3);
}

void NimBleSyncTransport::pumpChunk() {
    unsigned long now = millis();
    if (now - lastChunkMs_ < kChunkSpacingMs) return;

    uint8_t buf[ble::kChunkHeaderSize + ble::kMaxRecordsPerChunk * kRecordSize];
    size_t  limit = notifyPayloadLimit();
    if (limit > sizeof buf) limit = sizeof buf;

    size_t len = 0;
    if (!session_.nextChunk(buf, limit, len)) return;

    dataChar_->setValue(buf, len);
    dataChar_->notify();
    lastChunkMs_ = now;
}
