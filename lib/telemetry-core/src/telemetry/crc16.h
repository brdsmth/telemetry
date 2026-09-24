// crc16.h
//
// CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, no final xor.
// Check value for "123456789" is 0x29B1. Used by every wire format in
// schema/PROTOCOL.md.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace telemetry {

// Pass the previous result as `init` to continue a CRC across buffers.
uint16_t crc16CcittFalse(const uint8_t* data, size_t len, uint16_t init = 0xFFFF);

}  // namespace telemetry
