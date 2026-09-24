// memory_storage.h
//
// In-memory implementations of the storage interfaces the core depends on.
// Used by the host tests and by the simulator build. Deterministic: failures
// happen only when a test asks for them.
#pragma once
#include <stdint.h>
#include <string.h>

#include <vector>

#include "telemetry/ring_store.h"

namespace telemetry {
namespace testing {

class MemorySlotStorage : public ISlotStorage {
public:
    explicit MemorySlotStorage(uint32_t slots)
    : slots_(slots), data_(static_cast<size_t>(slots) * kRecordSize, 0) {}

    uint32_t slotCount() const override { return slots_; }

    bool readSlot(uint32_t index, uint8_t* out, size_t len) override {
        reads++;
        if (fail_reads || index >= slots_ || len != kRecordSize) return false;
        memcpy(out, &data_[static_cast<size_t>(index) * kRecordSize], len);
        return true;
    }

    bool writeSlot(uint32_t index, const uint8_t* data, size_t len) override {
        writes++;
        if (fail_writes || index >= slots_ || len != kRecordSize) return false;
        memcpy(&data_[static_cast<size_t>(index) * kRecordSize], data, len);
        return true;
    }

    // Test hook: scribble over a slot so it no longer decodes.
    void corruptSlot(uint32_t index) {
        for (size_t i = 0; i < kRecordSize; i++) {
            data_[static_cast<size_t>(index) * kRecordSize + i] ^= 0xA5;
        }
    }

    bool fail_reads  = false;
    bool fail_writes = false;
    uint32_t reads   = 0;
    uint32_t writes  = 0;

private:
    uint32_t             slots_;
    std::vector<uint8_t> data_;
};

class MemoryCursorStore : public ICursorStore {
public:
    bool load(RingCursor& out) override {
        loads++;
        if (!has_value) return false;
        out = value;
        return true;
    }

    bool save(const RingCursor& cursor) override {
        saves++;
        value     = cursor;
        has_value = true;
        return true;
    }

    bool       has_value = false;
    RingCursor value;
    uint32_t   loads = 0;
    uint32_t   saves = 0;
};

}  // namespace testing
}  // namespace telemetry
