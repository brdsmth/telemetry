// record.h
//
// The 20-byte reading record from schema/PROTOCOL.md §1. The same bytes live
// in flash, cross BLE, and travel inside the upload envelope.
//
// No Arduino or ESP-IDF dependencies: this compiles on the host and is tested
// against schema/vectors/records_vectors.h.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace telemetry {

constexpr uint8_t kProtocolVersion = 1;
constexpr uint8_t kRecordVersion   = 1;
constexpr size_t  kRecordSize      = 20;

// schema/PROTOCOL.md §1.1. Decoders accept ids not listed here.
enum class ReadingType : uint8_t {
    SoilResistanceOhms = 1,
    BatteryMillivolts  = 2,
    BoardTemperatureC  = 3,
};

// schema/PROTOCOL.md §1.2
enum class Quality : uint8_t {
    Ok   = 0,
    Low  = 1,
    High = 2,
    Open = 3,
};

// schema/PROTOCOL.md §1.3
enum RecordFlags : uint8_t {
    kFlagEpochValid = 0x01,
};

struct Record {
    uint8_t  version = kRecordVersion;
    uint8_t  type    = 0;
    uint8_t  quality = 0;
    uint8_t  flags   = 0;
    uint32_t seq     = 0;
    uint32_t time    = 0;   // unix seconds if epochValid(), else seconds since boot
    float    value   = 0.0f;
    uint16_t boot_id = 0;

    bool epochValid() const { return (flags & kFlagEpochValid) != 0; }
};

enum class DecodeError : uint8_t {
    None = 0,
    ShortInput,
    UnsupportedVersion,
    BadCrc,
};

// Names match the `error` strings in schema/vectors/records.json.
const char* decodeErrorName(DecodeError e);

// Writes exactly kRecordSize bytes to `out`, computing the CRC. The record's
// `version` field is written as-is so tests can produce invalid records.
void encodeRecord(const Record& r, uint8_t* out);

// Checks length, then version, then CRC, in that order. On success fills `out`.
DecodeError decodeRecord(const uint8_t* in, size_t len, Record& out);

const char* readingTypeName(uint8_t type);
const char* qualityName(uint8_t quality);

}  // namespace telemetry
