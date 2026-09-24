#include "telemetry/ble_codec.h"

#include <string.h>

#include "telemetry/crc16.h"

namespace telemetry {
namespace ble {

const char kServiceUuid[]        = "6eaedc54-f770-40e3-9806-a6ccf63c8099";
const char kDeviceInfoCharUuid[] = "6eaedc54-f770-40e3-0001-a6ccf63c8099";
const char kControlCharUuid[]    = "6eaedc54-f770-40e3-0002-a6ccf63c8099";
const char kDataCharUuid[]       = "6eaedc54-f770-40e3-0003-a6ccf63c8099";
const char kStatusCharUuid[]     = "6eaedc54-f770-40e3-0004-a6ccf63c8099";

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

}  // namespace

// --- advertising ---

void encodeAdvertising(const Advertising& a, uint8_t* out) {
    out[0] = a.protocol_version;
    putU16(out + 1, a.pending);
    putU16(out + 3, a.battery_mv);
    out[5] = a.adv_flags;
}

bool decodeAdvertising(const uint8_t* in, size_t len, Advertising& out) {
    if (len < kAdvertisingSize) return false;
    out.protocol_version = in[0];
    out.pending          = getU16(in + 1);
    out.battery_mv       = getU16(in + 3);
    out.adv_flags        = in[5];
    return true;
}

// --- device info ---

void encodeDeviceInfo(const DeviceInfo& d, uint8_t* out) {
    out[0] = d.protocol_version;
    out[1] = d.record_size;
    memcpy(out + 2, d.device_id, kDeviceIdSize);
    putU16(out + 8, d.boot_id);
    putU32(out + 10, d.next_seq);
    putU32(out + 14, d.collected_through);
    putU32(out + 18, d.secured_through);
    putU32(out + 22, d.uptime_s);
    putU32(out + 26, d.unix_time);
    putU32(out + 30, d.dropped);
    putU16(out + 34, d.battery_mv);
    putU16(out + 36, d.capacity);
    memcpy(out + 38, d.fw_version, kFirmwareVersionSize);
}

bool decodeDeviceInfo(const uint8_t* in, size_t len, DeviceInfo& out) {
    if (len < kDeviceInfoSize) return false;
    out.protocol_version = in[0];
    out.record_size      = in[1];
    memcpy(out.device_id, in + 2, kDeviceIdSize);
    out.boot_id           = getU16(in + 8);
    out.next_seq          = getU32(in + 10);
    out.collected_through = getU32(in + 14);
    out.secured_through   = getU32(in + 18);
    out.uptime_s          = getU32(in + 22);
    out.unix_time         = getU32(in + 26);
    out.dropped           = getU32(in + 30);
    out.battery_mv        = getU16(in + 34);
    out.capacity          = getU16(in + 36);
    memcpy(out.fw_version, in + 38, kFirmwareVersionSize);
    return true;
}

// --- control ---

const char* commandErrorName(CommandError e) {
    switch (e) {
        case CommandError::None:          return "none";
        case CommandError::BadLength:     return "bad_length";
        case CommandError::UnknownOpcode: return "unknown_opcode";
    }
    return "unknown";
}

namespace {

// Exact payload length (excluding the opcode byte) per opcode, or -1.
int payloadLength(uint8_t opcode) {
    switch (static_cast<Opcode>(opcode)) {
        case Opcode::OpenSession:  return static_cast<int>(kSessionIdSize);
        case Opcode::SetTime:      return 4;
        case Opcode::ReadFrom:     return 6;
        case Opcode::AckCollected: return 4;
        case Opcode::AckSecured:   return 4;
        case Opcode::CloseSession: return 0;
    }
    return -1;
}

}  // namespace

CommandError decodeCommand(const uint8_t* in, size_t len, Command& out) {
    if (len < 1) return CommandError::BadLength;
    int expected = payloadLength(in[0]);
    if (expected < 0) return CommandError::UnknownOpcode;
    if (len != 1 + static_cast<size_t>(expected)) return CommandError::BadLength;

    out = Command();
    out.opcode = in[0];
    const uint8_t* p = in + 1;
    switch (static_cast<Opcode>(in[0])) {
        case Opcode::OpenSession:
            memcpy(out.session_id, p, kSessionIdSize);
            break;
        case Opcode::SetTime:
            out.unix_time = getU32(p);
            break;
        case Opcode::ReadFrom:
            out.seq         = getU32(p);
            out.max_records = getU16(p + 4);
            break;
        case Opcode::AckCollected:
        case Opcode::AckSecured:
            out.through_seq = getU32(p);
            break;
        case Opcode::CloseSession:
            break;
    }
    return CommandError::None;
}

size_t encodeCommand(const Command& c, uint8_t* out) {
    int expected = payloadLength(c.opcode);
    if (expected < 0) return 0;
    out[0] = c.opcode;
    uint8_t* p = out + 1;
    switch (static_cast<Opcode>(c.opcode)) {
        case Opcode::OpenSession:
            memcpy(p, c.session_id, kSessionIdSize);
            break;
        case Opcode::SetTime:
            putU32(p, c.unix_time);
            break;
        case Opcode::ReadFrom:
            putU32(p, c.seq);
            putU16(p + 4, c.max_records);
            break;
        case Opcode::AckCollected:
        case Opcode::AckSecured:
            putU32(p, c.through_seq);
            break;
        case Opcode::CloseSession:
            break;
    }
    return 1 + static_cast<size_t>(expected);
}

// --- status ---

const char* resultName(Result r) {
    switch (r) {
        case Result::Ok:            return "ok";
        case Result::NoSession:     return "no_session";
        case Result::BadArgument:   return "bad_argument";
        case Result::SeqOutOfRange: return "seq_out_of_range";
        case Result::Busy:          return "busy";
    }
    return "unknown";
}

void encodeStatus(const Status& s, uint8_t* out) {
    out[0] = s.opcode;
    out[1] = s.result;
    putU32(out + 2, s.seq);
}

bool decodeStatus(const uint8_t* in, size_t len, Status& out) {
    if (len < kStatusSize) return false;
    out.opcode = in[0];
    out.result = in[1];
    out.seq    = getU32(in + 2);
    return true;
}

// --- data chunk ---

void encodeChunkHeader(const ChunkHeader& h, uint8_t* out) {
    putU32(out, h.first_seq);
    out[4] = h.count;
    out[5] = h.flags;
    putU16(out + 6, h.crc);
}

bool decodeChunkHeader(const uint8_t* in, size_t len, ChunkHeader& out) {
    if (len < kChunkHeaderSize) return false;
    out.first_seq = getU32(in);
    out.count     = in[4];
    out.flags     = in[5];
    out.crc       = getU16(in + 6);
    return true;
}

uint16_t chunkCrc(const uint8_t* records, size_t len) {
    return crc16CcittFalse(records, len);
}

size_t recordsPerChunk(size_t max_len) {
    if (max_len <= kChunkHeaderSize) return 0;
    size_t n = (max_len - kChunkHeaderSize) / kRecordSize;
    return n > kMaxRecordsPerChunk ? kMaxRecordsPerChunk : n;
}

}  // namespace ble
}  // namespace telemetry
