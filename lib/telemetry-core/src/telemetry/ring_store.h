// ring_store.h
//
// Fixed-slot ring buffer of encoded records with a persisted cursor. This is
// the sensor's local store from docs/ARCHITECTURE.md §3.1 and the two-level
// acknowledgement from docs/adr/0002.
//
// Slot index for a record is `seq % capacity`, so a seek by seq is O(1) and a
// record is never moved once written. The store holds seqs in
// [tail_seq, next_seq). `secured_through` is the only cursor that frees slots.
//
// The two interfaces below are what the core needs from the platform. The
// ESP32 implements them on LittleFS and NVS; tests and the simulator use the
// in-memory versions in telemetry/testing/memory_storage.h.
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "telemetry/record.h"

namespace telemetry {

class ISlotStorage {
public:
    virtual ~ISlotStorage() {}
    virtual uint32_t slotCount() const = 0;
    // Both return false on an I/O failure. `len` is always kRecordSize.
    virtual bool readSlot(uint32_t index, uint8_t* out, size_t len) = 0;
    virtual bool writeSlot(uint32_t index, const uint8_t* data, size_t len) = 0;
};

struct RingCursor {
    uint32_t next_seq          = 1;  // seq the next append receives
    uint32_t tail_seq          = 1;  // oldest seq still stored; == next_seq when empty
    uint32_t collected_through = 0;  // highest seq the phone holds; 0 = none
    uint32_t secured_through   = 0;  // highest seq the server holds; 0 = none
    uint32_t dropped           = 0;  // records overwritten before being secured
};

class ICursorStore {
public:
    virtual ~ICursorStore() {}
    // Returns false when nothing has been stored yet; `out` is left untouched.
    virtual bool load(RingCursor& out) = 0;
    virtual bool save(const RingCursor& cursor) = 0;
};

// What to do with a new record when every slot holds an unsecured record.
enum class FullPolicy : uint8_t {
    DropOldest,    // overwrite the oldest record and count it in `dropped`
    StopSampling,  // refuse the append; the caller decides what to do
};

enum class AppendResult : uint8_t {
    Stored,
    StoredAfterDrop,  // stored, but the oldest record was overwritten
    Full,             // StopSampling and no free slot
    StorageError,
};

class RingStore {
public:
    RingStore(ISlotStorage& slots, ICursorStore& cursor_store, FullPolicy policy);

    // Loads the cursor. A missing or inconsistent cursor resets the store to
    // empty; returns false in that case so the caller can log it.
    bool begin();

    // Assigns r.seq, encodes and stores it, and persists the cursor.
    AppendResult append(Record& r);

    // Fills `out` with up to `max` records having seq >= from_seq, in ascending
    // seq order, starting no earlier than the oldest stored record. Slots that
    // fail to decode are skipped and counted in corrupt(). Returns the count.
    size_t read(uint32_t from_seq, Record* out, size_t max);

    // Phone holds everything through `through_seq`. Frees nothing.
    // Returns false if through_seq names a record that does not exist yet.
    bool ackCollected(uint32_t through_seq);

    // Server holds everything through `through_seq`. Frees those slots and
    // raises collected_through to at least the same point.
    bool ackSecured(uint32_t through_seq);

    uint32_t capacity() const { return slots_.slotCount(); }
    uint32_t size() const { return cursor_.next_seq - cursor_.tail_seq; }
    uint32_t freeSlots() const { return capacity() - size(); }
    bool     empty() const { return size() == 0; }

    // Records stored but not yet collected by a phone.
    uint32_t pending() const;

    uint32_t corrupt() const { return corrupt_; }
    const RingCursor& cursor() const { return cursor_; }
    FullPolicy policy() const { return policy_; }

private:
    uint32_t slotFor(uint32_t seq) const { return seq % capacity(); }
    bool cursorIsConsistent(const RingCursor& c) const;

    ISlotStorage& slots_;
    ICursorStore& cursor_store_;
    FullPolicy    policy_;
    RingCursor    cursor_;
    uint32_t      corrupt_ = 0;
};

}  // namespace telemetry
