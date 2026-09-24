#include "telemetry/ring_store.h"

namespace telemetry {

RingStore::RingStore(ISlotStorage& slots, ICursorStore& cursor_store, FullPolicy policy)
: slots_(slots), cursor_store_(cursor_store), policy_(policy) {}

bool RingStore::cursorIsConsistent(const RingCursor& c) const {
    if (c.next_seq == 0 || c.tail_seq == 0) return false;
    if (c.tail_seq > c.next_seq) return false;
    if (c.next_seq - c.tail_seq > capacity()) return false;
    if (c.collected_through >= c.next_seq) return false;
    if (c.secured_through >= c.next_seq) return false;
    if (c.secured_through > c.collected_through) return false;
    // Everything secured must already have been freed.
    if (c.secured_through != 0 && c.secured_through >= c.tail_seq) return false;
    return true;
}

bool RingStore::begin() {
    RingCursor loaded;
    if (cursor_store_.load(loaded) && cursorIsConsistent(loaded)) {
        cursor_ = loaded;
        return true;
    }
    cursor_ = RingCursor();
    cursor_store_.save(cursor_);
    return false;
}

AppendResult RingStore::append(Record& r) {
    if (capacity() == 0) return AppendResult::StorageError;

    bool dropped = false;
    if (freeSlots() == 0) {
        if (policy_ == FullPolicy::StopSampling) return AppendResult::Full;
        // DropOldest: the record at tail_seq is by definition unsecured, since
        // secured records are freed as soon as they are acked.
        cursor_.tail_seq++;
        cursor_.dropped++;
        dropped = true;
    }

    r.seq = cursor_.next_seq;
    uint8_t buf[kRecordSize];
    encodeRecord(r, buf);
    if (!slots_.writeSlot(slotFor(r.seq), buf, kRecordSize)) {
        return AppendResult::StorageError;
    }

    cursor_.next_seq++;
    cursor_store_.save(cursor_);
    return dropped ? AppendResult::StoredAfterDrop : AppendResult::Stored;
}

size_t RingStore::read(uint32_t from_seq, Record* out, size_t max) {
    uint32_t seq = from_seq < cursor_.tail_seq ? cursor_.tail_seq : from_seq;
    size_t n = 0;
    uint8_t buf[kRecordSize];

    while (n < max && seq < cursor_.next_seq) {
        bool ok = slots_.readSlot(slotFor(seq), buf, kRecordSize) &&
                  decodeRecord(buf, kRecordSize, out[n]) == DecodeError::None &&
                  out[n].seq == seq;
        if (ok) {
            n++;
        } else {
            corrupt_++;
        }
        seq++;
    }
    return n;
}

bool RingStore::ackCollected(uint32_t through_seq) {
    if (through_seq >= cursor_.next_seq) return false;
    if (through_seq > cursor_.collected_through) {
        cursor_.collected_through = through_seq;
        cursor_store_.save(cursor_);
    }
    return true;
}

bool RingStore::ackSecured(uint32_t through_seq) {
    if (through_seq >= cursor_.next_seq) return false;
    if (through_seq <= cursor_.secured_through) return true;

    cursor_.secured_through = through_seq;
    if (cursor_.collected_through < through_seq) cursor_.collected_through = through_seq;
    if (cursor_.tail_seq <= through_seq) cursor_.tail_seq = through_seq + 1;
    cursor_store_.save(cursor_);
    return true;
}

uint32_t RingStore::pending() const {
    // First seq not yet collected, but never before the oldest stored record.
    uint32_t first = cursor_.collected_through + 1;
    if (first < cursor_.tail_seq) first = cursor_.tail_seq;
    return first < cursor_.next_seq ? cursor_.next_seq - first : 0;
}

}  // namespace telemetry
