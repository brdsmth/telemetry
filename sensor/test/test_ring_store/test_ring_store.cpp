// Behavioural tests for the ring store over in-memory storage.
#include <unity.h>

#include "telemetry/ring_store.h"
#include "telemetry/testing/memory_storage.h"

using namespace telemetry;
using telemetry::testing::MemoryCursorStore;
using telemetry::testing::MemorySlotStorage;

void setUp() {}
void tearDown() {}

static Record soil(float ohms, uint32_t uptime_s) {
    Record r;
    r.type    = static_cast<uint8_t>(ReadingType::SoilResistanceOhms);
    r.quality = static_cast<uint8_t>(Quality::Ok);
    r.time    = uptime_s;
    r.value   = ohms;
    r.boot_id = 1;
    return r;
}

static void appendN(RingStore& store, uint32_t n, AppendResult expect = AppendResult::Stored) {
    for (uint32_t i = 0; i < n; i++) {
        Record r = soil(1000.0f * (i + 1), 60 * i);
        TEST_ASSERT_EQUAL(expect, store.append(r));
    }
}

// --- empty store ---

void test_fresh_store_is_empty() {
    MemorySlotStorage slots(8);
    MemoryCursorStore cursor;
    RingStore store(slots, cursor, FullPolicy::DropOldest);

    TEST_ASSERT_FALSE(store.begin());  // nothing persisted yet
    TEST_ASSERT_TRUE(store.empty());
    TEST_ASSERT_EQUAL_UINT32(0, store.size());
    TEST_ASSERT_EQUAL_UINT32(8, store.freeSlots());
    TEST_ASSERT_EQUAL_UINT32(0, store.pending());
    TEST_ASSERT_EQUAL_UINT32(1, store.cursor().next_seq);
    TEST_ASSERT_EQUAL_UINT32(1, store.cursor().tail_seq);

    Record out[4];
    TEST_ASSERT_EQUAL(0, store.read(1, out, 4));
}

// --- append and read ---

void test_append_assigns_sequence_from_one() {
    MemorySlotStorage slots(8);
    MemoryCursorStore cursor;
    RingStore store(slots, cursor, FullPolicy::DropOldest);
    store.begin();

    Record a = soil(100.0f, 0);
    Record b = soil(200.0f, 60);
    TEST_ASSERT_EQUAL(AppendResult::Stored, store.append(a));
    TEST_ASSERT_EQUAL(AppendResult::Stored, store.append(b));
    TEST_ASSERT_EQUAL_UINT32(1, a.seq);
    TEST_ASSERT_EQUAL_UINT32(2, b.seq);
    TEST_ASSERT_EQUAL_UINT32(2, store.size());
    TEST_ASSERT_EQUAL_UINT32(2, store.pending());
}

void test_read_returns_records_in_order_from_seq() {
    MemorySlotStorage slots(8);
    MemoryCursorStore cursor;
    RingStore store(slots, cursor, FullPolicy::DropOldest);
    store.begin();
    appendN(store, 5);

    Record out[8];
    size_t n = store.read(1, out, 8);
    TEST_ASSERT_EQUAL(5, n);
    for (size_t i = 0; i < n; i++) {
        TEST_ASSERT_EQUAL_UINT32(i + 1, out[i].seq);
        TEST_ASSERT_EQUAL_FLOAT(1000.0f * (i + 1), out[i].value);
    }

    n = store.read(4, out, 8);
    TEST_ASSERT_EQUAL(2, n);
    TEST_ASSERT_EQUAL_UINT32(4, out[0].seq);
    TEST_ASSERT_EQUAL_UINT32(5, out[1].seq);
}

void test_read_honours_max() {
    MemorySlotStorage slots(8);
    MemoryCursorStore cursor;
    RingStore store(slots, cursor, FullPolicy::DropOldest);
    store.begin();
    appendN(store, 5);

    Record out[2];
    TEST_ASSERT_EQUAL(2, store.read(1, out, 2));
    TEST_ASSERT_EQUAL_UINT32(1, out[0].seq);
    TEST_ASSERT_EQUAL_UINT32(2, out[1].seq);
}

void test_read_past_end_returns_nothing() {
    MemorySlotStorage slots(8);
    MemoryCursorStore cursor;
    RingStore store(slots, cursor, FullPolicy::DropOldest);
    store.begin();
    appendN(store, 3);

    Record out[2];
    TEST_ASSERT_EQUAL(0, store.read(4, out, 2));
    TEST_ASSERT_EQUAL(0, store.read(1000, out, 2));
}

// --- persistence ---

void test_cursor_survives_restart() {
    MemorySlotStorage slots(8);
    MemoryCursorStore cursor;
    {
        RingStore store(slots, cursor, FullPolicy::DropOldest);
        store.begin();
        appendN(store, 4);
        TEST_ASSERT_TRUE(store.ackCollected(2));
    }
    RingStore again(slots, cursor, FullPolicy::DropOldest);
    TEST_ASSERT_TRUE(again.begin());
    TEST_ASSERT_EQUAL_UINT32(4, again.size());
    TEST_ASSERT_EQUAL_UINT32(5, again.cursor().next_seq);
    TEST_ASSERT_EQUAL_UINT32(2, again.cursor().collected_through);
    TEST_ASSERT_EQUAL_UINT32(2, again.pending());

    Record out[8];
    TEST_ASSERT_EQUAL(4, again.read(1, out, 8));
    TEST_ASSERT_EQUAL_UINT32(1, out[0].seq);
}

void test_inconsistent_cursor_resets_to_empty() {
    MemorySlotStorage slots(8);
    MemoryCursorStore cursor;
    cursor.has_value       = true;
    cursor.value.next_seq  = 5;
    cursor.value.tail_seq  = 9;  // tail after head: impossible

    RingStore store(slots, cursor, FullPolicy::DropOldest);
    TEST_ASSERT_FALSE(store.begin());
    TEST_ASSERT_TRUE(store.empty());
    TEST_ASSERT_EQUAL_UINT32(1, store.cursor().next_seq);
    TEST_ASSERT_EQUAL_UINT32(1, cursor.saves);  // reset was persisted
}

void test_cursor_saved_on_every_mutation() {
    MemorySlotStorage slots(8);
    MemoryCursorStore cursor;
    RingStore store(slots, cursor, FullPolicy::DropOldest);
    store.begin();
    uint32_t after_begin = cursor.saves;

    appendN(store, 3);
    TEST_ASSERT_EQUAL_UINT32(after_begin + 3, cursor.saves);
    store.ackCollected(2);
    TEST_ASSERT_EQUAL_UINT32(after_begin + 4, cursor.saves);
    store.ackSecured(1);
    TEST_ASSERT_EQUAL_UINT32(after_begin + 5, cursor.saves);
}

// --- acknowledgements ---

void test_ack_collected_reduces_pending_but_frees_nothing() {
    MemorySlotStorage slots(8);
    MemoryCursorStore cursor;
    RingStore store(slots, cursor, FullPolicy::DropOldest);
    store.begin();
    appendN(store, 5);

    TEST_ASSERT_TRUE(store.ackCollected(3));
    TEST_ASSERT_EQUAL_UINT32(2, store.pending());
    TEST_ASSERT_EQUAL_UINT32(5, store.size());
    TEST_ASSERT_EQUAL_UINT32(1, store.cursor().tail_seq);
}

void test_ack_secured_frees_slots_and_raises_collected() {
    MemorySlotStorage slots(8);
    MemoryCursorStore cursor;
    RingStore store(slots, cursor, FullPolicy::DropOldest);
    store.begin();
    appendN(store, 5);

    TEST_ASSERT_TRUE(store.ackSecured(3));
    TEST_ASSERT_EQUAL_UINT32(3, store.cursor().secured_through);
    TEST_ASSERT_EQUAL_UINT32(3, store.cursor().collected_through);
    TEST_ASSERT_EQUAL_UINT32(4, store.cursor().tail_seq);
    TEST_ASSERT_EQUAL_UINT32(2, store.size());
    TEST_ASSERT_EQUAL_UINT32(6, store.freeSlots());

    // Reading from below the tail starts at the tail.
    Record out[8];
    TEST_ASSERT_EQUAL(2, store.read(1, out, 8));
    TEST_ASSERT_EQUAL_UINT32(4, out[0].seq);
    TEST_ASSERT_EQUAL_UINT32(5, out[1].seq);
}

void test_ack_of_future_seq_is_rejected() {
    MemorySlotStorage slots(8);
    MemoryCursorStore cursor;
    RingStore store(slots, cursor, FullPolicy::DropOldest);
    store.begin();
    appendN(store, 2);

    TEST_ASSERT_FALSE(store.ackCollected(3));
    TEST_ASSERT_FALSE(store.ackSecured(3));
    TEST_ASSERT_EQUAL_UINT32(0, store.cursor().collected_through);
    TEST_ASSERT_EQUAL_UINT32(2, store.size());
}

void test_acks_are_idempotent_and_never_move_backwards() {
    MemorySlotStorage slots(8);
    MemoryCursorStore cursor;
    RingStore store(slots, cursor, FullPolicy::DropOldest);
    store.begin();
    appendN(store, 5);

    TEST_ASSERT_TRUE(store.ackSecured(4));
    uint32_t saves = cursor.saves;
    TEST_ASSERT_TRUE(store.ackSecured(4));
    TEST_ASSERT_TRUE(store.ackSecured(2));
    TEST_ASSERT_TRUE(store.ackCollected(1));
    TEST_ASSERT_EQUAL_UINT32(4, store.cursor().secured_through);
    TEST_ASSERT_EQUAL_UINT32(4, store.cursor().collected_through);
    TEST_ASSERT_EQUAL_UINT32(5, store.cursor().tail_seq);
    TEST_ASSERT_EQUAL_UINT32(saves, cursor.saves);  // no-ops do not touch NVS
}

void test_ack_zero_means_none() {
    MemorySlotStorage slots(8);
    MemoryCursorStore cursor;
    RingStore store(slots, cursor, FullPolicy::DropOldest);
    store.begin();
    appendN(store, 2);

    TEST_ASSERT_TRUE(store.ackCollected(0));
    TEST_ASSERT_TRUE(store.ackSecured(0));
    TEST_ASSERT_EQUAL_UINT32(2, store.pending());
    TEST_ASSERT_EQUAL_UINT32(2, store.size());
}

// --- full ring ---

void test_drop_oldest_overwrites_and_counts() {
    MemorySlotStorage slots(4);
    MemoryCursorStore cursor;
    RingStore store(slots, cursor, FullPolicy::DropOldest);
    store.begin();
    appendN(store, 4);
    appendN(store, 2, AppendResult::StoredAfterDrop);

    TEST_ASSERT_EQUAL_UINT32(4, store.size());
    TEST_ASSERT_EQUAL_UINT32(3, store.cursor().tail_seq);
    TEST_ASSERT_EQUAL_UINT32(7, store.cursor().next_seq);
    TEST_ASSERT_EQUAL_UINT32(2, store.cursor().dropped);

    Record out[8];
    TEST_ASSERT_EQUAL(4, store.read(1, out, 8));
    TEST_ASSERT_EQUAL_UINT32(3, out[0].seq);
    TEST_ASSERT_EQUAL_UINT32(6, out[3].seq);
}

void test_stop_sampling_refuses_when_full() {
    MemorySlotStorage slots(4);
    MemoryCursorStore cursor;
    RingStore store(slots, cursor, FullPolicy::StopSampling);
    store.begin();
    appendN(store, 4);

    Record r = soil(5.0f, 0);
    TEST_ASSERT_EQUAL(AppendResult::Full, store.append(r));
    TEST_ASSERT_EQUAL_UINT32(4, store.size());
    TEST_ASSERT_EQUAL_UINT32(5, store.cursor().next_seq);
    TEST_ASSERT_EQUAL_UINT32(0, store.cursor().dropped);

    // Securing makes room again.
    TEST_ASSERT_TRUE(store.ackSecured(2));
    TEST_ASSERT_EQUAL(AppendResult::Stored, store.append(r));
    TEST_ASSERT_EQUAL_UINT32(5, r.seq);
}

void test_pending_after_drops() {
    MemorySlotStorage slots(4);
    MemoryCursorStore cursor;
    RingStore store(slots, cursor, FullPolicy::DropOldest);
    store.begin();
    appendN(store, 4);
    appendN(store, 2, AppendResult::StoredAfterDrop);
    // Stored: 3,4,5,6. Nothing collected. Pending must be what is still here.
    TEST_ASSERT_EQUAL_UINT32(4, store.pending());
    TEST_ASSERT_TRUE(store.ackCollected(4));
    TEST_ASSERT_EQUAL_UINT32(2, store.pending());
}

void test_wraparound_after_secure() {
    MemorySlotStorage slots(4);
    MemoryCursorStore cursor;
    RingStore store(slots, cursor, FullPolicy::StopSampling);
    store.begin();

    for (uint32_t round = 0; round < 3; round++) {
        appendN(store, 4);
        TEST_ASSERT_TRUE(store.ackSecured(store.cursor().next_seq - 1));
        TEST_ASSERT_TRUE(store.empty());
    }
    appendN(store, 3);

    Record out[8];
    TEST_ASSERT_EQUAL(3, store.read(1, out, 8));
    TEST_ASSERT_EQUAL_UINT32(13, out[0].seq);
    TEST_ASSERT_EQUAL_UINT32(14, out[1].seq);
    TEST_ASSERT_EQUAL_UINT32(15, out[2].seq);
}

// --- storage faults ---

void test_corrupt_slot_is_skipped_and_counted() {
    MemorySlotStorage slots(8);
    MemoryCursorStore cursor;
    RingStore store(slots, cursor, FullPolicy::DropOldest);
    store.begin();
    appendN(store, 4);
    slots.corruptSlot(2 % 8);  // seq 2

    Record out[8];
    TEST_ASSERT_EQUAL(3, store.read(1, out, 8));
    TEST_ASSERT_EQUAL_UINT32(1, out[0].seq);
    TEST_ASSERT_EQUAL_UINT32(3, out[1].seq);
    TEST_ASSERT_EQUAL_UINT32(4, out[2].seq);
    TEST_ASSERT_EQUAL_UINT32(1, store.corrupt());
}

void test_write_failure_does_not_advance_cursor() {
    MemorySlotStorage slots(8);
    MemoryCursorStore cursor;
    RingStore store(slots, cursor, FullPolicy::DropOldest);
    store.begin();
    appendN(store, 2);
    uint32_t saves = cursor.saves;

    slots.fail_writes = true;
    Record r = soil(1.0f, 0);
    TEST_ASSERT_EQUAL(AppendResult::StorageError, store.append(r));
    TEST_ASSERT_EQUAL_UINT32(2, store.size());
    TEST_ASSERT_EQUAL_UINT32(3, store.cursor().next_seq);
    TEST_ASSERT_EQUAL_UINT32(saves, cursor.saves);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_fresh_store_is_empty);
    RUN_TEST(test_append_assigns_sequence_from_one);
    RUN_TEST(test_read_returns_records_in_order_from_seq);
    RUN_TEST(test_read_honours_max);
    RUN_TEST(test_read_past_end_returns_nothing);
    RUN_TEST(test_cursor_survives_restart);
    RUN_TEST(test_inconsistent_cursor_resets_to_empty);
    RUN_TEST(test_cursor_saved_on_every_mutation);
    RUN_TEST(test_ack_collected_reduces_pending_but_frees_nothing);
    RUN_TEST(test_ack_secured_frees_slots_and_raises_collected);
    RUN_TEST(test_ack_of_future_seq_is_rejected);
    RUN_TEST(test_acks_are_idempotent_and_never_move_backwards);
    RUN_TEST(test_ack_zero_means_none);
    RUN_TEST(test_drop_oldest_overwrites_and_counts);
    RUN_TEST(test_stop_sampling_refuses_when_full);
    RUN_TEST(test_pending_after_drops);
    RUN_TEST(test_wraparound_after_secure);
    RUN_TEST(test_corrupt_slot_is_skipped_and_counted);
    RUN_TEST(test_write_failure_does_not_advance_cursor);
    return UNITY_END();
}
