#include "telemetry/record.h"

#include <string.h>

#include "telemetry/crc16.h"

namespace telemetry {

namespace {

void putU16(uint8_t* p, uint16_t v) {
    p[0] = static_cast<uint8_t>(v);
    p[1] = static_cast<uint8_t>(v >> 8);
}

void putU32(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>(v);
    p[1] = static_cast<uint8_t>(v >> 8);
    p[2] = static_cast<uint8_t>(v >> 16);
    p[3] = static_cast<uint8_t>(v >> 24);
}

uint16_t getU16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

uint32_t getU32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

uint32_t floatBits(float f) {
    uint32_t u;
    memcpy(&u, &f, sizeof u);
    return u;
}

float bitsFloat(uint32_t u) {
    float f;
    memcpy(&f, &u, sizeof f);
    return f;
}

constexpr size_t kCrcOffset = kRecordSize - 2;

}  // namespace

const char* decodeErrorName(DecodeError e) {
    switch (e) {
        case DecodeError::None:               return "none";
        case DecodeError::ShortInput:         return "short_input";
        case DecodeError::UnsupportedVersion: return "unsupported_version";
        case DecodeError::BadCrc:             return "bad_crc";
    }
    return "unknown";
}

void encodeRecord(const Record& r, uint8_t* out) {
    out[0] = r.version;
    out[1] = r.type;
    out[2] = r.quality;
    out[3] = r.flags;
    putU32(out + 4, r.seq);
    putU32(out + 8, r.time);
    putU32(out + 12, floatBits(r.value));
    putU16(out + 16, r.boot_id);
    putU16(out + kCrcOffset, crc16CcittFalse(out, kCrcOffset));
}

DecodeError decodeRecord(const uint8_t* in, size_t len, Record& out) {
    if (len < kRecordSize) return DecodeError::ShortInput;
    if (in[0] != kRecordVersion) return DecodeError::UnsupportedVersion;
    if (crc16CcittFalse(in, kCrcOffset) != getU16(in + kCrcOffset)) return DecodeError::BadCrc;

    out.version = in[0];
    out.type    = in[1];
    out.quality = in[2];
    out.flags   = in[3];
    out.seq     = getU32(in + 4);
    out.time    = getU32(in + 8);
    out.value   = bitsFloat(getU32(in + 12));
    out.boot_id = getU16(in + 16);
    return DecodeError::None;
}

const char* readingTypeName(uint8_t type) {
    switch (static_cast<ReadingType>(type)) {
        case ReadingType::SoilResistanceOhms: return "soil_resistance_ohms";
        case ReadingType::BatteryMillivolts:  return "battery_millivolts";
        case ReadingType::BoardTemperatureC:  return "board_temperature_c";
    }
    return "unknown";
}

const char* qualityName(uint8_t quality) {
    switch (static_cast<Quality>(quality)) {
        case Quality::Ok:   return "ok";
        case Quality::Low:  return "low";
        case Quality::High: return "high";
        case Quality::Open: return "open";
    }
    return "unknown";
}

}  // namespace telemetry
