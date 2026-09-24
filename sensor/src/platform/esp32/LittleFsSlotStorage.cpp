#include "platform/esp32/LittleFsSlotStorage.h"

#include <LittleFS.h>

#include "logger.h"

using telemetry::kRecordSize;

LittleFsSlotStorage::LittleFsSlotStorage(const char* path, uint32_t slots)
: path_(path), slots_(slots) {}

bool LittleFsSlotStorage::begin() {
    const size_t want = static_cast<size_t>(slots_) * kRecordSize;

    if (LittleFS.exists(path_)) {
        File f = LittleFS.open(path_, "r");
        size_t have = f ? f.size() : 0;
        if (f) f.close();
        if (have != want) {
            logln("-----> Ring file is " + String(have) + " bytes, want " + String(want) + ", recreating");
            LittleFS.remove(path_);
        }
    }

    if (!LittleFS.exists(path_)) {
        if (!createZeroed()) return false;
        recreated_ = true;
    }

    file_ = LittleFS.open(path_, "r+");
    if (!file_) {
        logln("-----> ERROR: could not open ring file");
        return false;
    }
    logln("-----> Ring file " + String(path_) + ": " + String(slots_) + " slots, " + String(want) + " bytes");
    return true;
}

bool LittleFsSlotStorage::createZeroed() {
    File f = LittleFS.open(path_, "w");
    if (!f) {
        logln("-----> ERROR: could not create ring file");
        return false;
    }
    static uint8_t zeros[1024] = {0};
    size_t remaining = static_cast<size_t>(slots_) * kRecordSize;
    while (remaining > 0) {
        size_t n = remaining < sizeof zeros ? remaining : sizeof zeros;
        if (f.write(zeros, n) != n) {
            f.close();
            logln("-----> ERROR: short write creating ring file");
            return false;
        }
        remaining -= n;
    }
    f.close();
    return true;
}

bool LittleFsSlotStorage::readSlot(uint32_t index, uint8_t* out, size_t len) {
    if (!file_ || index >= slots_ || len != kRecordSize) return false;
    if (!file_.seek(static_cast<uint32_t>(index) * kRecordSize, SeekSet)) return false;
    return file_.read(out, len) == len;
}

bool LittleFsSlotStorage::writeSlot(uint32_t index, const uint8_t* data, size_t len) {
    if (!file_ || index >= slots_ || len != kRecordSize) return false;
    if (!file_.seek(static_cast<uint32_t>(index) * kRecordSize, SeekSet)) return false;
    if (file_.write(data, len) != len) return false;
    file_.flush();
    return true;
}
