#include "platform/esp32/NvsCursorStore.h"

#include <string.h>

#include "logger.h"

static const char*    kNamespace = "tlmring";
static const char*    kKey       = "cur";
static const uint32_t kMagic     = 0x52494e47;  // "RING"

NvsCursorStore::NvsCursorStore(uint32_t capacity) : capacity_(capacity) {}

bool NvsCursorStore::begin() {
    if (!prefs_.begin(kNamespace, /*readOnly=*/false)) {
        logln("-----> ERROR: NVS namespace open failed");
        return false;
    }
    return true;
}

bool NvsCursorStore::load(telemetry::RingCursor& out) {
    Blob b;
    size_t n = prefs_.getBytes(kKey, &b, sizeof b);
    if (n != sizeof b) return false;
    if (b.magic != kMagic) return false;
    if (b.capacity != capacity_) {
        logln("-----> Ring capacity changed (" + String(b.capacity) + " -> " + String(capacity_) + "), cursor discarded");
        return false;
    }
    out = b.cursor;
    return true;
}

bool NvsCursorStore::save(const telemetry::RingCursor& cursor) {
    Blob b;
    b.magic    = kMagic;
    b.capacity = capacity_;
    b.cursor   = cursor;
    return prefs_.putBytes(kKey, &b, sizeof b) == sizeof b;
}

void NvsCursorStore::clear() {
    prefs_.remove(kKey);
}
