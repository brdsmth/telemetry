// ble_codec.h
//
// Byte layouts for the BLE sync service, schema/PROTOCOL.md §3. Pure codec:
// no BLE stack, no session logic. Tested against schema/vectors/ble_vectors.h.
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "telemetry/record.h"

namespace telemetry {
namespace ble {

extern const char kServiceUuid[];
extern const char kDeviceInfoCharUuid[];
extern const char kControlCharUuid[];
extern const char kDataCharUuid[];
extern const char kStatusCharUuid[];

constexpr size_t   kAdvertisingSize     = 6;
constexpr size_t   kDeviceInfoSize      = 54;
constexpr size_t   kStatusSize          = 6;
constexpr size_t   kChunkHeaderSize     = 8;
constexpr uint8_t  kMaxRecordsPerChunk  = 24;
constexpr size_t   kSessionIdSize       = 16;
constexpr size_t   kFirmwareVersionSize = 16;
constexpr size_t   kDeviceIdSize        = 6;
constexpr uint16_t kManufacturerId      = 0xFFFF;  // Bluetooth SIG test id
constexpr uint16_t kMtu                 = 512;

// --- advertising (§3.1) ---

enum AdvFlags : uint8_t {
    kAdvTimeSet = 0x01,
    kAdvDropped = 0x02,
};

struct Advertising {
    uint8_t  protocol_version = kProtocolVersion;
    uint16_t pending          = 0;
    uint16_t battery_mv       = 0;
    uint8_t  adv_flags        = 0;
};

void encodeAdvertising(const Advertising& a, uint8_t* out);
bool decodeAdvertising(const uint8_t* in, size_t len, Advertising& out);

// --- device info (§3.2) ---

struct DeviceInfo {
    uint8_t  protocol_version = kProtocolVersion;
    uint8_t  record_size      = kRecordSize;
    uint8_t  device_id[kDeviceIdSize] = {0, 0, 0, 0, 0, 0};
    uint16_t boot_id           = 0;
    uint32_t next_seq          = 0;
    uint32_t collected_through = 0;
    uint32_t secured_through   = 0;
    uint32_t uptime_s          = 0;
    uint32_t unix_time         = 0;
    uint32_t dropped           = 0;
    uint16_t battery_mv        = 0;
    uint16_t capacity          = 0;
    char     fw_version[kFirmwareVersionSize] = {0};  // NUL padded, not necessarily terminated
};

void encodeDeviceInfo(const DeviceInfo& d, uint8_t* out);
bool decodeDeviceInfo(const uint8_t* in, size_t len, DeviceInfo& out);

// --- control (§3.3) ---

enum class Opcode : uint8_t {
    OpenSession  = 0x01,
    SetTime      = 0x02,
    ReadFrom     = 0x03,
    AckCollected = 0x04,
    AckSecured   = 0x05,
    CloseSession = 0x06,
};

// Fields not used by the opcode are left zero.
struct Command {
    uint8_t  opcode = 0;
    uint8_t  session_id[kSessionIdSize] = {0};
    uint32_t unix_time   = 0;
    uint32_t seq         = 0;
    uint16_t max_records = 0;
    uint32_t through_seq = 0;
};

enum class CommandError : uint8_t {
    None = 0,
    BadLength,
    UnknownOpcode,
};

const char* commandErrorName(CommandError e);

CommandError decodeCommand(const uint8_t* in, size_t len, Command& out);

// Returns the encoded length, 0 for an unknown opcode. `out` must hold at
// least kMaxCommandSize bytes.
constexpr size_t kMaxCommandSize = 1 + kSessionIdSize;
size_t encodeCommand(const Command& c, uint8_t* out);

// --- status (§3.5) ---

enum class Result : uint8_t {
    Ok            = 0,
    NoSession     = 1,
    BadArgument   = 2,
    SeqOutOfRange = 3,
    Busy          = 4,
};

const char* resultName(Result r);

struct Status {
    uint8_t  opcode = 0;
    uint8_t  result = 0;
    uint32_t seq    = 0;
};

void encodeStatus(const Status& s, uint8_t* out);
bool decodeStatus(const uint8_t* in, size_t len, Status& out);

// --- data chunk (§3.4) ---

enum ChunkFlags : uint8_t {
    kChunkLast = 0x01,
};

struct ChunkHeader {
    uint32_t first_seq = 0;
    uint8_t  count     = 0;
    uint8_t  flags     = 0;
    uint16_t crc       = 0;  // over the record bytes that follow the header
};

void encodeChunkHeader(const ChunkHeader& h, uint8_t* out);
bool decodeChunkHeader(const uint8_t* in, size_t len, ChunkHeader& out);

// CRC over `count` encoded records placed back to back.
uint16_t chunkCrc(const uint8_t* records, size_t len);

// How many whole records fit in a notification of `max_len` bytes, capped at
// kMaxRecordsPerChunk.
size_t recordsPerChunk(size_t max_len);

}  // namespace ble
}  // namespace telemetry
