// LittleFsSlotStorage.h
//
// ISlotStorage over one fixed-size file on LittleFS. Slot i is bytes
// [i * kRecordSize, (i + 1) * kRecordSize). LittleFS is power-loss safe at
// the file level, which is what the ring store needs; a raw partition can
// replace this later behind the same interface.
#pragma once
#include <FS.h>

#include "telemetry/ring_store.h"

class LittleFsSlotStorage : public telemetry::ISlotStorage {
public:
    LittleFsSlotStorage(const char* path, uint32_t slots);

    // LittleFS must already be mounted. Creates the file zero filled if it is
    // missing or has the wrong size (the cursor store detects a capacity change
    // and resets the ring in that case).
    bool begin();

    uint32_t slotCount() const override { return slots_; }
    bool readSlot(uint32_t index, uint8_t* out, size_t len) override;
    bool writeSlot(uint32_t index, const uint8_t* data, size_t len) override;

    // Slots the file was (re)created with on this boot, for logging.
    bool recreated() const { return recreated_; }

private:
    bool createZeroed();

    const char* path_;
    uint32_t    slots_;
    File        file_;
    bool        recreated_ = false;
};
