// NvsCursorStore.h
//
// ICursorStore over NVS via Preferences. The cursor is one blob so each save
// is a single NVS entry; NVS wear levels across the partition.
#pragma once
#include <Preferences.h>

#include "telemetry/ring_store.h"

class NvsCursorStore : public telemetry::ICursorStore {
public:
    // `capacity` is stored with the cursor. A cursor saved for a different
    // capacity is treated as absent, since seq -> slot mapping changed.
    explicit NvsCursorStore(uint32_t capacity);

    bool begin();

    bool load(telemetry::RingCursor& out) override;
    bool save(const telemetry::RingCursor& cursor) override;

    // Forget the persisted cursor (bench / factory reset).
    void clear();

private:
    struct Blob {
        uint32_t              magic;
        uint32_t              capacity;
        telemetry::RingCursor cursor;
    };

    Preferences prefs_;
    uint32_t    capacity_;
};
