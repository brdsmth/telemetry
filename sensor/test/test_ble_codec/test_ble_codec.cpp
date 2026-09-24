// Conformance test for the BLE payload codec against schema/vectors.
#include <string.h>
#include <unity.h>

#include "ble_vectors.h"
#include "telemetry/ble_codec.h"

using namespace telemetry;
using namespace telemetry::ble;

void setUp() {}
void tearDown() {}

void test_constants_match_vectors() {
    TEST_ASSERT_EQUAL(BLE_VECTOR_PROTOCOL_VERSION, kProtocolVersion);
    TEST_ASSERT_EQUAL_STRING(BLE_VECTOR_SERVICE_UUID, kServiceUuid);
    TEST_ASSERT_EQUAL_STRING(BLE_VECTOR_CHAR_DEVICE_INFO, kDeviceInfoCharUuid);
    TEST_ASSERT_EQUAL_STRING(BLE_VECTOR_CHAR_CONTROL, kControlCharUuid);
    TEST_ASSERT_EQUAL_STRING(BLE_VECTOR_CHAR_DATA, kDataCharUuid);
    TEST_ASSERT_EQUAL_STRING(BLE_VECTOR_CHAR_STATUS, kStatusCharUuid);
    TEST_ASSERT_EQUAL(BLE_VECTOR_ADVERTISING_SIZE, kAdvertisingSize);
    TEST_ASSERT_EQUAL(BLE_VECTOR_DEVICE_INFO_SIZE, kDeviceInfoSize);
    TEST_ASSERT_EQUAL(BLE_VECTOR_STATUS_SIZE, kStatusSize);
    TEST_ASSERT_EQUAL(BLE_VECTOR_CHUNK_HEADER_SIZE, kChunkHeaderSize);
    TEST_ASSERT_EQUAL(BLE_VECTOR_MAX_RECORDS_PER_CHUNK, kMaxRecordsPerChunk);
}

void test_advertising_vectors() {
    for (size_t i = 0; i < kAdvertisingVectorCount; i++) {
        const AdvertisingVector& v = kAdvertisingVectors[i];
        Advertising a;
        a.protocol_version = v.protocol_version;
        a.pending          = v.pending;
        a.battery_mv       = v.battery_mv;
        a.adv_flags        = v.adv_flags;
        uint8_t out[kAdvertisingSize];
        encodeAdvertising(a, out);
        TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(v.bytes, out, kAdvertisingSize, v.name);

        Advertising back;
        TEST_ASSERT_TRUE_MESSAGE(decodeAdvertising(v.bytes, kAdvertisingSize, back), v.name);
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(v.pending, back.pending, v.name);
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(v.battery_mv, back.battery_mv, v.name);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(v.adv_flags, back.adv_flags, v.name);
    }
}

void test_device_info_vectors() {
    for (size_t i = 0; i < kDeviceInfoVectorCount; i++) {
        const DeviceInfoVector& v = kDeviceInfoVectors[i];
        DeviceInfo d;
        d.protocol_version = v.protocol_version;
        d.record_size      = v.record_size;
        memcpy(d.device_id, v.device_id, 6);
        d.boot_id           = v.boot_id;
        d.next_seq          = v.next_seq;
        d.collected_through = v.collected_through;
        d.secured_through   = v.secured_through;
        d.uptime_s          = v.uptime_s;
        d.unix_time         = v.unix_time;
        d.dropped           = v.dropped;
        d.battery_mv        = v.battery_mv;
        d.capacity          = v.capacity;
        memset(d.fw_version, 0, sizeof d.fw_version);
        strncpy(d.fw_version, v.fw_version, sizeof d.fw_version);

        uint8_t out[kDeviceInfoSize];
        encodeDeviceInfo(d, out);
        TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(v.bytes, out, kDeviceInfoSize, v.name);

        DeviceInfo back;
        TEST_ASSERT_TRUE_MESSAGE(decodeDeviceInfo(v.bytes, kDeviceInfoSize, back), v.name);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(v.next_seq, back.next_seq, v.name);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(v.secured_through, back.secured_through, v.name);
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(v.capacity, back.capacity, v.name);
        TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(v.device_id, back.device_id, 6, v.name);
        TEST_ASSERT_EQUAL_MEMORY_MESSAGE(d.fw_version, back.fw_version, kFirmwareVersionSize, v.name);
    }
}

void test_status_vectors() {
    for (size_t i = 0; i < kStatusVectorCount; i++) {
        const StatusVector& v = kStatusVectors[i];
        Status s;
        s.opcode = v.opcode;
        s.result = v.result;
        s.seq    = v.seq;
        uint8_t out[kStatusSize];
        encodeStatus(s, out);
        TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(v.bytes, out, kStatusSize, v.name);

        Status back;
        TEST_ASSERT_TRUE_MESSAGE(decodeStatus(v.bytes, kStatusSize, back), v.name);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(v.opcode, back.opcode, v.name);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(v.result, back.result, v.name);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(v.seq, back.seq, v.name);
    }
}

void test_command_vectors_decode() {
    for (size_t i = 0; i < kCommandVectorCount; i++) {
        const CommandVector& v = kCommandVectors[i];
        Command c;
        TEST_ASSERT_EQUAL_MESSAGE(CommandError::None, decodeCommand(v.bytes, v.len, c), v.name);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(v.opcode, c.opcode, v.name);
        TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(v.session_id, c.session_id, kSessionIdSize, v.name);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(v.unix_time, c.unix_time, v.name);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(v.seq, c.seq, v.name);
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(v.max_records, c.max_records, v.name);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(v.through_seq, c.through_seq, v.name);
    }
}

void test_command_vectors_encode() {
    for (size_t i = 0; i < kCommandVectorCount; i++) {
        const CommandVector& v = kCommandVectors[i];
        Command c;
        c.opcode = v.opcode;
        memcpy(c.session_id, v.session_id, kSessionIdSize);
        c.unix_time   = v.unix_time;
        c.seq         = v.seq;
        c.max_records = v.max_records;
        c.through_seq = v.through_seq;

        uint8_t out[kMaxCommandSize];
        size_t n = encodeCommand(c, out);
        TEST_ASSERT_EQUAL_MESSAGE(v.len, n, v.name);
        TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(v.bytes, out, n, v.name);
    }
}

void test_invalid_command_vectors() {
    for (size_t i = 0; i < kInvalidCommandVectorCount; i++) {
        const InvalidCommandVector& v = kInvalidCommandVectors[i];
        Command c;
        CommandError err = decodeCommand(v.bytes, v.len, c);
        TEST_ASSERT_NOT_EQUAL_MESSAGE(CommandError::None, err, v.name);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(v.error, commandErrorName(err), v.name);
    }
    Command unknown;
    unknown.opcode = 0x7f;
    uint8_t out[kMaxCommandSize];
    TEST_ASSERT_EQUAL(0, encodeCommand(unknown, out));
}

void test_chunk_vectors() {
    for (size_t i = 0; i < kChunkVectorCount; i++) {
        const ChunkVector& v = kChunkVectors[i];
        ChunkHeader h;
        TEST_ASSERT_TRUE_MESSAGE(decodeChunkHeader(v.bytes, v.len, h), v.name);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(v.first_seq, h.first_seq, v.name);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(v.count, h.count, v.name);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(v.flags, h.flags, v.name);
        TEST_ASSERT_EQUAL_HEX16_MESSAGE(v.crc, h.crc, v.name);

        size_t body_len = v.len - kChunkHeaderSize;
        TEST_ASSERT_EQUAL_MESSAGE(h.count * kRecordSize, body_len, v.name);
        TEST_ASSERT_EQUAL_HEX16_MESSAGE(v.crc, chunkCrc(v.bytes + kChunkHeaderSize, body_len), v.name);

        uint8_t out[kChunkHeaderSize];
        encodeChunkHeader(h, out);
        TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(v.bytes, out, kChunkHeaderSize, v.name);

        for (uint8_t r = 0; r < h.count; r++) {
            Record rec;
            TEST_ASSERT_EQUAL_MESSAGE(DecodeError::None,
                decodeRecord(v.bytes + kChunkHeaderSize + r * kRecordSize, kRecordSize, rec), v.name);
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(h.first_seq + r, rec.seq, v.name);
        }
    }
}

void test_records_per_chunk() {
    TEST_ASSERT_EQUAL(0, recordsPerChunk(0));
    TEST_ASSERT_EQUAL(0, recordsPerChunk(kChunkHeaderSize + kRecordSize - 1));
    TEST_ASSERT_EQUAL(1, recordsPerChunk(kChunkHeaderSize + kRecordSize));
    TEST_ASSERT_EQUAL(3, recordsPerChunk(68));
    TEST_ASSERT_EQUAL(kMaxRecordsPerChunk, recordsPerChunk(512));
    TEST_ASSERT_EQUAL(kMaxRecordsPerChunk, recordsPerChunk(4096));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_constants_match_vectors);
    RUN_TEST(test_advertising_vectors);
    RUN_TEST(test_device_info_vectors);
    RUN_TEST(test_status_vectors);
    RUN_TEST(test_command_vectors_decode);
    RUN_TEST(test_command_vectors_encode);
    RUN_TEST(test_invalid_command_vectors);
    RUN_TEST(test_chunk_vectors);
    RUN_TEST(test_records_per_chunk);
    return UNITY_END();
}
