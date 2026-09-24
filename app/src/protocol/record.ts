// The 20-byte reading record, schema/PROTOCOL.md §1. Same bytes in the
// sensor's flash, over BLE, and base64 inside the upload envelope.
import { readF32, readU16, readU32, writeF32, writeU16, writeU32 } from './bytes';
import { crc16 } from './crc16';

export const PROTOCOL_VERSION = 1;
export const RECORD_VERSION = 1;
export const RECORD_SIZE = 20;

export const FLAG_EPOCH_VALID = 0x01;

export const ReadingType = {
  SoilResistanceOhms: 1,
  BatteryMillivolts: 2,
  BoardTemperatureC: 3,
} as const;

export const Quality = { Ok: 0, Low: 1, High: 2, Open: 3 } as const;

export interface Record {
  version: number;
  type: number;
  quality: number;
  flags: number;
  seq: number;
  /** Unix seconds when epochValid, else seconds since the boot named by bootId. */
  time: number;
  value: number;
  bootId: number;
}

export type DecodeError = 'short_input' | 'unsupported_version' | 'bad_crc';

export class RecordDecodeError extends Error {
  constructor(public readonly code: DecodeError) {
    super(code);
    this.name = 'RecordDecodeError';
  }
}

export function epochValid(r: Record): boolean {
  return (r.flags & FLAG_EPOCH_VALID) !== 0;
}

export function encodeRecord(r: Record): Uint8Array {
  const out = new Uint8Array(RECORD_SIZE);
  out[0] = r.version;
  out[1] = r.type;
  out[2] = r.quality;
  out[3] = r.flags;
  writeU32(out, 4, r.seq);
  writeU32(out, 8, r.time);
  writeF32(out, 12, r.value);
  writeU16(out, 16, r.bootId);
  writeU16(out, 18, crc16(out.subarray(0, 18)));
  return out;
}

/** Checks length, then version, then CRC, in that order. */
export function decodeRecord(b: Uint8Array): Record {
  if (b.length < RECORD_SIZE) throw new RecordDecodeError('short_input');
  if (b[0] !== RECORD_VERSION) throw new RecordDecodeError('unsupported_version');
  if (crc16(b.subarray(0, 18)) !== readU16(b, 18)) throw new RecordDecodeError('bad_crc');
  return {
    version: b[0],
    type: b[1],
    quality: b[2],
    flags: b[3],
    seq: readU32(b, 4),
    time: readU32(b, 8),
    value: readF32(b, 12),
    bootId: readU16(b, 16),
  };
}

export function readingTypeName(type: number): string {
  switch (type) {
    case ReadingType.SoilResistanceOhms:
      return 'soil_resistance_ohms';
    case ReadingType.BatteryMillivolts:
      return 'battery_millivolts';
    case ReadingType.BoardTemperatureC:
      return 'board_temperature_c';
    default:
      return 'unknown';
  }
}

export function qualityName(quality: number): string {
  switch (quality) {
    case Quality.Ok:
      return 'ok';
    case Quality.Low:
      return 'low';
    case Quality.High:
      return 'high';
    case Quality.Open:
      return 'open';
    default:
      return 'unknown';
  }
}
