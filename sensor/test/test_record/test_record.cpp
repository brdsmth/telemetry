// Conformance test for the record codec against schema/vectors.
#include <string.h>
#include <unity.h>

#include "records_vectors.h"
#include "telemetry/crc16.h"
#include "telemetry/record.h"

using namespace telemetry;

void setUp() {}
void tearDown() {}

void test_crc_check_value() {
    const char* s = "123456789";
    TEST_ASSERT_EQUAL_HEX16(0x29B1, crc16CcittFalse(reinterpret_cast<const uint8_t*>(s), strlen(s)));
}

void test_crc_continues_across_buffers() {
    const uint8_t a[] = {'1', '2', '3', '4'};
    const uint8_t b[] = {'5', '6', '7', '8', '9'};
    uint16_t partial = crc16CcittFalse(a, sizeof a);
    TEST_ASSERT_EQUAL_HEX16(0x29B1, crc16CcittFalse(b, sizeof b, partial));
}

void test_constants_match_vectors() {
    TEST_ASSERT_EQUAL(RECORD_VECTOR_VERSION, kRecordVersion);
    TEST_ASSERT_EQUAL(RECORD_VECTOR_SIZE, kRecordSize);
}

void test_decode_valid_vectors() {
    for (size_t i = 0; i < kRecordVectorCount; i++) {
        const RecordVector& v = kRecordVectors[i];
        Record r;
        DecodeError err = decodeRecord(v.bytes, kRecordSize, r);
        TEST_ASSERT_EQUAL_MESSAGE(DecodeError::None, err, v.name);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(v.version, r.version, v.name);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(v.type, r.type, v.name);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(v.quality, r.quality, v.name);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(v.flags, r.flags, v.name);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(v.seq, r.seq, v.name);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(v.time, r.time, v.name);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(v.value, r.value, v.name);
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(v.boot_id, r.boot_id, v.name);
    }
}

void test_encode_valid_vectors_byte_for_byte() {
    for (size_t i = 0; i < kRecordVectorCount; i++) {
        const RecordVector& v = kRecordVectors[i];
        Record r;
        r.version = v.version;
        r.type    = v.type;
        r.quality = v.quality;
        r.flags   = v.flags;
        r.seq     = v.seq;
        r.time    = v.time;
        r.value   = v.value;
        r.boot_id = v.boot_id;

        uint8_t out[kRecordSize];
        encodeRecord(r, out);
        TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(v.bytes, out, kRecordSize, v.name);
    }
}

void test_invalid_vectors_report_named_error() {
    for (size_t i = 0; i < kInvalidRecordVectorCount; i++) {
        const InvalidRecordVector& v = kInvalidRecordVectors[i];
        Record r;
        DecodeError err = decodeRecord(v.bytes, v.len, r);
        TEST_ASSERT_NOT_EQUAL_MESSAGE(DecodeError::None, err, v.name);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(v.error, decodeErrorName(err), v.name);
    }
}

void test_roundtrip_arbitrary_record() {
    Record in;
    in.type    = static_cast<uint8_t>(ReadingType::SoilResistanceOhms);
    in.quality = static_cast<uint8_t>(Quality::High);
    in.flags   = kFlagEpochValid;
    in.seq     = 123456;
    in.time    = 1790121600u;
    in.value   = 123456.75f;
    in.boot_id = 9;

    uint8_t buf[kRecordSize];
    encodeRecord(in, buf);

    Record out;
    TEST_ASSERT_EQUAL(DecodeError::None, decodeRecord(buf, sizeof buf, out));
    TEST_ASSERT_EQUAL_UINT32(in.seq, out.seq);
    TEST_ASSERT_EQUAL_UINT32(in.time, out.time);
    TEST_ASSERT_EQUAL_FLOAT(in.value, out.value);
    TEST_ASSERT_EQUAL_UINT16(in.boot_id, out.boot_id);
    TEST_ASSERT_TRUE(out.epochValid());
}

void test_names() {
    TEST_ASSERT_EQUAL_STRING("soil_resistance_ohms", readingTypeName(1));
    TEST_ASSERT_EQUAL_STRING("battery_millivolts", readingTypeName(2));
    TEST_ASSERT_EQUAL_STRING("unknown", readingTypeName(250));
    TEST_ASSERT_EQUAL_STRING("ok", qualityName(0));
    TEST_ASSERT_EQUAL_STRING("open", qualityName(3));
    TEST_ASSERT_EQUAL_STRING("unknown", qualityName(9));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_crc_check_value);
    RUN_TEST(test_crc_continues_across_buffers);
    RUN_TEST(test_constants_match_vectors);
    RUN_TEST(test_decode_valid_vectors);
    RUN_TEST(test_encode_valid_vectors_byte_for_byte);
    RUN_TEST(test_invalid_vectors_report_named_error);
    RUN_TEST(test_roundtrip_arbitrary_record);
    RUN_TEST(test_names);
    return UNITY_END();
}
