// Behavioural tests for the sensor side of the sync protocol, driven the way
// a BLE adapter would drive it: write a command, read the status, pull chunks.
#include <string.h>
#include <unity.h>

#include "telemetry/ble_codec.h"
#include "telemetry/ring_store.h"
#include "telemetry/sync_session.h"
#include "telemetry/testing/fake_system.h"
#include "telemetry/testing/memory_storage.h"

using namespace telemetry;
using namespace telemetry::ble;
using telemetry::testing::FakeSystem;
using telemetry::testing::MemoryCursorStore;
using telemetry::testing::MemorySlotStorage;

void setUp() {}
void tearDown() {}

// --- harness -------------------------------------------------------------

struct Rig {
    MemorySlotStorage slots;
    MemoryCursorStore cursor;
    FakeSystem        system;
    RingStore         store;
    SyncSession       session;

    explicit Rig(uint32_t capacity = 64, FullPolicy policy = FullPolicy::DropOldest)
    : slots(capacity), store(slots, cursor, policy), session(store, system) {
        store.begin();
    }

    void seed(uint32_t n) {
        for (uint32_t i = 0; i < n; i++) {
            Record r;
            r.type    = static_cast<uint8_t>(ReadingType::SoilResistanceOhms);
            r.value   = 1000.0f + i;
            r.time    = 60 * i;
            r.boot_id = system.boot_id;
            store.append(r);
        }
    }

    Status send(const Command& c) {
        uint8_t buf[kMaxCommandSize];
        size_t n = encodeCommand(c, buf);
        Status s;
        session.handleControl(buf, n, s);
        return s;
    }

    Status open() {
        Command c;
        c.opcode = static_cast<uint8_t>(Opcode::OpenSession);
        for (size_t i = 0; i < kSessionIdSize; i++) c.session_id[i] = static_cast<uint8_t>(i);
        return send(c);
    }

    Status readFrom(uint32_t seq, uint16_t max_records = 0) {
        Command c;
        c.opcode      = static_cast<uint8_t>(Opcode::ReadFrom);
        c.seq         = seq;
        c.max_records = max_records;
        return send(c);
    }

    Status ack(Opcode op, uint32_t through) {
        Command c;
        c.opcode      = static_cast<uint8_t>(op);
        c.through_seq = through;
        return send(c);
    }

    Status setTime(uint32_t t) {
        Command c;
        c.opcode    = static_cast<uint8_t>(Opcode::SetTime);
        c.unix_time = t;
        return send(c);
    }

    Status close() {
        Command c;
        c.opcode = static_cast<uint8_t>(Opcode::CloseSession);
        return send(c);
    }

    // Pulls one chunk, verifies its CRC and decodes its records.
    bool pull(size_t max_len, ChunkHeader& h, Record* records) {
        uint8_t buf[512];
        size_t len = 0;
        if (!session.nextChunk(buf, max_len, len)) return false;
        TEST_ASSERT_TRUE(len <= max_len);
        TEST_ASSERT_TRUE(decodeChunkHeader(buf, len, h));
        TEST_ASSERT_EQUAL(kChunkHeaderSize + h.count * kRecordSize, len);
        TEST_ASSERT_EQUAL_HEX16(h.crc, chunkCrc(buf + kChunkHeaderSize, h.count * kRecordSize));
        for (uint8_t i = 0; i < h.count; i++) {
            TEST_ASSERT_EQUAL(DecodeError::None,
                decodeRecord(buf + kChunkHeaderSize + i * kRecordSize, kRecordSize, records[i]));
        }
        return true;
    }
};

static void assertStatus(const Status& s, Opcode op, Result r) {
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(op), s.opcode);
    TEST_ASSERT_EQUAL_STRING(resultName(r), resultName(static_cast<Result>(s.result)));
}

// --- sessions ------------------------------------------------------------

void test_commands_before_open_are_refused() {
    Rig rig;
    rig.seed(3);
    assertStatus(rig.readFrom(1), Opcode::ReadFrom, Result::NoSession);
    assertStatus(rig.setTime(1790121600u), Opcode::SetTime, Result::NoSession);
    assertStatus(rig.ack(Opcode::AckCollected, 1), Opcode::AckCollected, Result::NoSession);
    TEST_ASSERT_FALSE(rig.session.sessionOpen());
    TEST_ASSERT_EQUAL_UINT32(0, rig.system.set_time_calls);
}

void test_open_session_reports_next_seq_and_keeps_id() {
    Rig rig;
    rig.seed(5);
    Status s = rig.open();
    assertStatus(s, Opcode::OpenSession, Result::Ok);
    TEST_ASSERT_EQUAL_UINT32(6, s.seq);
    TEST_ASSERT_TRUE(rig.session.sessionOpen());
    TEST_ASSERT_EQUAL_UINT8(3, rig.session.sessionId()[3]);
    TEST_ASSERT_EQUAL_UINT32(1, rig.session.stats().sessions_opened);
}

void test_close_and_disconnect_end_the_session() {
    Rig rig;
    rig.seed(2);
    rig.open();
    assertStatus(rig.close(), Opcode::CloseSession, Result::Ok);
    TEST_ASSERT_FALSE(rig.session.sessionOpen());
    assertStatus(rig.readFrom(1), Opcode::ReadFrom, Result::NoSession);

    rig.open();
    rig.readFrom(1);
    TEST_ASSERT_TRUE(rig.session.streaming());
    rig.session.onDisconnect();
    TEST_ASSERT_FALSE(rig.session.sessionOpen());
    TEST_ASSERT_FALSE(rig.session.streaming());
    uint8_t buf[512];
    size_t len;
    TEST_ASSERT_FALSE(rig.session.nextChunk(buf, 512, len));
}

void test_malformed_commands_get_bad_argument_with_opcode_echoed() {
    Rig rig;
    rig.open();

    const uint8_t short_set_time[] = {0x02, 0x01, 0x02};
    Status s;
    rig.session.handleControl(short_set_time, sizeof short_set_time, s);
    assertStatus(s, Opcode::SetTime, Result::BadArgument);

    const uint8_t unknown[] = {0x7f, 0, 0, 0, 0};
    rig.session.handleControl(unknown, sizeof unknown, s);
    TEST_ASSERT_EQUAL_UINT8(0x7f, s.opcode);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Result::BadArgument), s.result);

    rig.session.handleControl(unknown, 0, s);
    TEST_ASSERT_EQUAL_UINT8(0, s.opcode);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Result::BadArgument), s.result);

    TEST_ASSERT_EQUAL_UINT32(3, rig.session.stats().bad_commands);
    TEST_ASSERT_TRUE(rig.session.sessionOpen());  // a bad command does not drop the session
}

// --- time ----------------------------------------------------------------

void test_set_time_sets_the_clock() {
    Rig rig;
    rig.open();
    Status s = rig.setTime(1790121600u);
    assertStatus(s, Opcode::SetTime, Result::Ok);
    TEST_ASSERT_EQUAL_UINT32(1790121600u, s.seq);
    TEST_ASSERT_EQUAL_UINT32(1790121600u, rig.system.unixTime());
    rig.system.advance(10);
    TEST_ASSERT_EQUAL_UINT32(1790121610u, rig.system.unixTime());
}

void test_set_time_zero_or_rejected_is_bad_argument() {
    Rig rig;
    rig.open();
    assertStatus(rig.setTime(0), Opcode::SetTime, Result::BadArgument);
    rig.system.reject_set_time = true;
    assertStatus(rig.setTime(1790121600u), Opcode::SetTime, Result::BadArgument);
    TEST_ASSERT_EQUAL_UINT32(0, rig.system.unixTime());
}

// --- streaming -----------------------------------------------------------

void test_read_all_streams_full_then_last_chunk() {
    Rig rig;
    rig.seed(30);
    rig.open();
    Status s = rig.readFrom(1);
    assertStatus(s, Opcode::ReadFrom, Result::Ok);
    TEST_ASSERT_EQUAL_UINT32(1, s.seq);

    ChunkHeader h;
    Record recs[kMaxRecordsPerChunk];

    TEST_ASSERT_TRUE(rig.pull(512, h, recs));
    TEST_ASSERT_EQUAL_UINT32(1, h.first_seq);
    TEST_ASSERT_EQUAL_UINT8(24, h.count);
    TEST_ASSERT_EQUAL_UINT8(0, h.flags);
    TEST_ASSERT_EQUAL_UINT32(24, recs[23].seq);
    TEST_ASSERT_EQUAL_FLOAT(1000.0f, recs[0].value);

    TEST_ASSERT_TRUE(rig.pull(512, h, recs));
    TEST_ASSERT_EQUAL_UINT32(25, h.first_seq);
    TEST_ASSERT_EQUAL_UINT8(6, h.count);
    TEST_ASSERT_EQUAL_UINT8(kChunkLast, h.flags);

    TEST_ASSERT_FALSE(rig.pull(512, h, recs));
    TEST_ASSERT_FALSE(rig.session.streaming());
    TEST_ASSERT_EQUAL_UINT32(2, rig.session.stats().chunks);
    TEST_ASSERT_EQUAL_UINT32(30, rig.session.stats().records_streamed);
}

void test_read_exactly_one_chunk_marks_last() {
    Rig rig;
    rig.seed(24);
    rig.open();
    rig.readFrom(1);

    ChunkHeader h;
    Record recs[kMaxRecordsPerChunk];
    TEST_ASSERT_TRUE(rig.pull(512, h, recs));
    TEST_ASSERT_EQUAL_UINT8(24, h.count);
    TEST_ASSERT_EQUAL_UINT8(kChunkLast, h.flags);
    TEST_ASSERT_FALSE(rig.pull(512, h, recs));
}

void test_read_window_limits_records_and_marks_last() {
    Rig rig;
    rig.seed(30);
    rig.open();
    rig.readFrom(3, 5);

    ChunkHeader h;
    Record recs[kMaxRecordsPerChunk];
    TEST_ASSERT_TRUE(rig.pull(512, h, recs));
    TEST_ASSERT_EQUAL_UINT32(3, h.first_seq);
    TEST_ASSERT_EQUAL_UINT8(5, h.count);
    TEST_ASSERT_EQUAL_UINT8(kChunkLast, h.flags);
    TEST_ASSERT_EQUAL_UINT32(7, recs[4].seq);
    TEST_ASSERT_FALSE(rig.pull(512, h, recs));

    // The phone continues with the next window.
    rig.readFrom(8, 5);
    TEST_ASSERT_TRUE(rig.pull(512, h, recs));
    TEST_ASSERT_EQUAL_UINT32(8, h.first_seq);
    TEST_ASSERT_EQUAL_UINT8(5, h.count);
}

void test_small_mtu_packs_fewer_records_per_chunk() {
    Rig rig;
    rig.seed(7);
    rig.open();
    rig.readFrom(1);

    ChunkHeader h;
    Record recs[kMaxRecordsPerChunk];
    TEST_ASSERT_TRUE(rig.pull(68, h, recs));  // (68 - 8) / 20 = 3
    TEST_ASSERT_EQUAL_UINT8(3, h.count);
    TEST_ASSERT_TRUE(rig.pull(68, h, recs));
    TEST_ASSERT_EQUAL_UINT8(3, h.count);
    TEST_ASSERT_TRUE(rig.pull(68, h, recs));
    TEST_ASSERT_EQUAL_UINT8(1, h.count);
    TEST_ASSERT_EQUAL_UINT8(kChunkLast, h.flags);
}

void test_read_from_below_tail_clamps_to_tail() {
    Rig rig(4);
    rig.seed(6);  // seqs 3..6 remain, 1 and 2 dropped
    rig.open();
    Status s = rig.readFrom(1);
    TEST_ASSERT_EQUAL_UINT32(3, s.seq);

    ChunkHeader h;
    Record recs[kMaxRecordsPerChunk];
    TEST_ASSERT_TRUE(rig.pull(512, h, recs));
    TEST_ASSERT_EQUAL_UINT32(3, h.first_seq);
    TEST_ASSERT_EQUAL_UINT8(4, h.count);
    TEST_ASSERT_EQUAL_UINT8(kChunkLast, h.flags);
}

void test_read_beyond_end_yields_empty_last_chunk() {
    Rig rig;
    rig.seed(3);
    rig.open();
    assertStatus(rig.readFrom(4), Opcode::ReadFrom, Result::Ok);

    ChunkHeader h;
    Record recs[kMaxRecordsPerChunk];
    TEST_ASSERT_TRUE(rig.pull(512, h, recs));
    TEST_ASSERT_EQUAL_UINT32(4, h.first_seq);
    TEST_ASSERT_EQUAL_UINT8(0, h.count);
    TEST_ASSERT_EQUAL_UINT8(kChunkLast, h.flags);
    TEST_ASSERT_FALSE(rig.pull(512, h, recs));
}

void test_empty_store_yields_empty_last_chunk() {
    Rig rig;
    rig.open();
    rig.readFrom(1);
    ChunkHeader h;
    Record recs[kMaxRecordsPerChunk];
    TEST_ASSERT_TRUE(rig.pull(512, h, recs));
    TEST_ASSERT_EQUAL_UINT8(0, h.count);
    TEST_ASSERT_EQUAL_UINT8(kChunkLast, h.flags);
}

void test_gap_from_corrupt_slot_ends_chunk_early() {
    Rig rig;
    rig.seed(10);
    rig.slots.corruptSlot(5 % 64);  // seq 5 unreadable
    rig.open();
    rig.readFrom(1);

    ChunkHeader h;
    Record recs[kMaxRecordsPerChunk];
    TEST_ASSERT_TRUE(rig.pull(512, h, recs));
    TEST_ASSERT_EQUAL_UINT32(1, h.first_seq);
    TEST_ASSERT_EQUAL_UINT8(4, h.count);  // 1..4
    TEST_ASSERT_EQUAL_UINT8(0, h.flags);

    TEST_ASSERT_TRUE(rig.pull(512, h, recs));
    TEST_ASSERT_EQUAL_UINT32(6, h.first_seq);  // gap at 5 is visible to the phone
    TEST_ASSERT_EQUAL_UINT8(5, h.count);       // 6..10
    TEST_ASSERT_EQUAL_UINT8(kChunkLast, h.flags);
}

void test_new_read_from_restarts_stream() {
    Rig rig;
    rig.seed(30);
    rig.open();
    rig.readFrom(1);
    ChunkHeader h;
    Record recs[kMaxRecordsPerChunk];
    TEST_ASSERT_TRUE(rig.pull(512, h, recs));
    TEST_ASSERT_EQUAL_UINT32(1, h.first_seq);

    rig.readFrom(20);
    TEST_ASSERT_TRUE(rig.pull(512, h, recs));
    TEST_ASSERT_EQUAL_UINT32(20, h.first_seq);
    TEST_ASSERT_EQUAL_UINT8(11, h.count);
    TEST_ASSERT_EQUAL_UINT8(kChunkLast, h.flags);
}

void test_chunk_too_small_for_a_record_sends_nothing() {
    Rig rig;
    rig.seed(3);
    rig.open();
    rig.readFrom(1);
    uint8_t buf[64];
    size_t len = 0;
    TEST_ASSERT_FALSE(rig.session.nextChunk(buf, 20, len));
    TEST_ASSERT_TRUE(rig.session.streaming());  // still pending for a larger MTU
}

// --- acknowledgements ------------------------------------------------------

void test_ack_collected_then_secured_moves_cursors() {
    Rig rig;
    rig.seed(10);
    rig.open();

    Status s = rig.ack(Opcode::AckCollected, 7);
    assertStatus(s, Opcode::AckCollected, Result::Ok);
    TEST_ASSERT_EQUAL_UINT32(7, s.seq);
    TEST_ASSERT_EQUAL_UINT32(10, rig.store.size());
    TEST_ASSERT_EQUAL_UINT32(3, rig.store.pending());

    s = rig.ack(Opcode::AckSecured, 4);
    assertStatus(s, Opcode::AckSecured, Result::Ok);
    TEST_ASSERT_EQUAL_UINT32(4, s.seq);
    TEST_ASSERT_EQUAL_UINT32(6, rig.store.size());
    TEST_ASSERT_EQUAL_UINT32(5, rig.store.cursor().tail_seq);
}

void test_ack_beyond_next_seq_is_out_of_range() {
    Rig rig;
    rig.seed(3);
    rig.open();
    Status s = rig.ack(Opcode::AckSecured, 4);
    assertStatus(s, Opcode::AckSecured, Result::SeqOutOfRange);
    TEST_ASSERT_EQUAL_UINT32(0, s.seq);  // current cursor is reported
    TEST_ASSERT_EQUAL_UINT32(3, rig.store.size());
}

void test_full_visit_pull_ack_collected_then_next_visit_secured() {
    Rig rig(16);
    rig.seed(10);

    // Visit one: pull everything, ack collected.
    rig.open();
    rig.setTime(1790121600u);
    rig.readFrom(1);
    ChunkHeader h;
    Record recs[kMaxRecordsPerChunk];
    TEST_ASSERT_TRUE(rig.pull(512, h, recs));
    TEST_ASSERT_EQUAL_UINT8(10, h.count);
    rig.ack(Opcode::AckCollected, recs[h.count - 1].seq);
    rig.close();
    TEST_ASSERT_EQUAL_UINT32(0, rig.store.pending());
    TEST_ASSERT_EQUAL_UINT32(10, rig.store.size());

    // Sensor keeps sampling while the phone is away.
    rig.seed(4);
    TEST_ASSERT_EQUAL_UINT32(4, rig.store.pending());

    // Visit two: server confirmed 1..10, phone pulls the new ones.
    rig.open();
    Status s = rig.ack(Opcode::AckSecured, 10);
    assertStatus(s, Opcode::AckSecured, Result::Ok);
    TEST_ASSERT_EQUAL_UINT32(4, rig.store.size());
    rig.readFrom(11);
    TEST_ASSERT_TRUE(rig.pull(512, h, recs));
    TEST_ASSERT_EQUAL_UINT32(11, h.first_seq);
    TEST_ASSERT_EQUAL_UINT8(4, h.count);
    TEST_ASSERT_EQUAL_UINT8(kChunkLast, h.flags);
}

// --- device info and advertising -----------------------------------------------

void test_device_info_reflects_store_and_system() {
    Rig rig(4096);
    rig.seed(20);
    rig.store.ackCollected(15);
    rig.store.ackSecured(10);
    rig.system.boot_id    = 7;
    rig.system.uptime_s   = 3600;
    rig.system.battery_mv = 3650;
    rig.system.setUnixTime(1790121600u);

    DeviceInfo d;
    rig.session.fillDeviceInfo(d);
    TEST_ASSERT_EQUAL_UINT8(kProtocolVersion, d.protocol_version);
    TEST_ASSERT_EQUAL_UINT8(kRecordSize, d.record_size);
    TEST_ASSERT_EQUAL_HEX8(0xaa, d.device_id[0]);
    TEST_ASSERT_EQUAL_HEX8(0xff, d.device_id[5]);
    TEST_ASSERT_EQUAL_UINT16(7, d.boot_id);
    TEST_ASSERT_EQUAL_UINT32(21, d.next_seq);
    TEST_ASSERT_EQUAL_UINT32(15, d.collected_through);
    TEST_ASSERT_EQUAL_UINT32(10, d.secured_through);
    TEST_ASSERT_EQUAL_UINT32(3600, d.uptime_s);
    TEST_ASSERT_EQUAL_UINT32(1790121600u, d.unix_time);
    TEST_ASSERT_EQUAL_UINT32(0, d.dropped);
    TEST_ASSERT_EQUAL_UINT16(3650, d.battery_mv);
    TEST_ASSERT_EQUAL_UINT16(4096, d.capacity);
    TEST_ASSERT_EQUAL_STRING_LEN("sensor-0.3.0", d.fw_version, 12);
    TEST_ASSERT_EQUAL_UINT8(0, d.fw_version[12]);

    uint8_t buf[kDeviceInfoSize];
    rig.session.encodeDeviceInfo(buf);
    DeviceInfo back;
    TEST_ASSERT_TRUE(decodeDeviceInfo(buf, sizeof buf, back));
    TEST_ASSERT_EQUAL_UINT32(21, back.next_seq);
}

void test_capacity_saturates_in_device_info() {
    Rig rig(70000);
    DeviceInfo d;
    rig.session.fillDeviceInfo(d);
    TEST_ASSERT_EQUAL_UINT16(0xFFFF, d.capacity);
}

void test_advertising_reflects_pending_time_and_drops() {
    Rig rig(4);
    Advertising a;

    rig.session.fillAdvertising(a);
    TEST_ASSERT_EQUAL_UINT8(kProtocolVersion, a.protocol_version);
    TEST_ASSERT_EQUAL_UINT16(0, a.pending);
    TEST_ASSERT_EQUAL_UINT8(0, a.adv_flags);

    rig.seed(3);
    rig.system.battery_mv = 3712;
    rig.system.setUnixTime(1790121600u);
    rig.session.fillAdvertising(a);
    TEST_ASSERT_EQUAL_UINT16(3, a.pending);
    TEST_ASSERT_EQUAL_UINT16(3712, a.battery_mv);
    TEST_ASSERT_EQUAL_UINT8(kAdvTimeSet, a.adv_flags);

    rig.seed(3);  // overflows a 4-slot ring: 2 dropped
    rig.session.fillAdvertising(a);
    TEST_ASSERT_EQUAL_UINT16(4, a.pending);
    TEST_ASSERT_EQUAL_UINT8(kAdvTimeSet | kAdvDropped, a.adv_flags);

    uint8_t buf[kAdvertisingSize];
    rig.session.encodeAdvertising(buf);
    Advertising back;
    TEST_ASSERT_TRUE(decodeAdvertising(buf, sizeof buf, back));
    TEST_ASSERT_EQUAL_UINT16(4, back.pending);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_commands_before_open_are_refused);
    RUN_TEST(test_open_session_reports_next_seq_and_keeps_id);
    RUN_TEST(test_close_and_disconnect_end_the_session);
    RUN_TEST(test_malformed_commands_get_bad_argument_with_opcode_echoed);
    RUN_TEST(test_set_time_sets_the_clock);
    RUN_TEST(test_set_time_zero_or_rejected_is_bad_argument);
    RUN_TEST(test_read_all_streams_full_then_last_chunk);
    RUN_TEST(test_read_exactly_one_chunk_marks_last);
    RUN_TEST(test_read_window_limits_records_and_marks_last);
    RUN_TEST(test_small_mtu_packs_fewer_records_per_chunk);
    RUN_TEST(test_read_from_below_tail_clamps_to_tail);
    RUN_TEST(test_read_beyond_end_yields_empty_last_chunk);
    RUN_TEST(test_empty_store_yields_empty_last_chunk);
    RUN_TEST(test_gap_from_corrupt_slot_ends_chunk_early);
    RUN_TEST(test_new_read_from_restarts_stream);
    RUN_TEST(test_chunk_too_small_for_a_record_sends_nothing);
    RUN_TEST(test_ack_collected_then_secured_moves_cursors);
    RUN_TEST(test_ack_beyond_next_seq_is_out_of_range);
    RUN_TEST(test_full_visit_pull_ack_collected_then_next_visit_secured);
    RUN_TEST(test_device_info_reflects_store_and_system);
    RUN_TEST(test_capacity_saturates_in_device_info);
    RUN_TEST(test_advertising_reflects_pending_time_and_drops);
    return UNITY_END();
}
