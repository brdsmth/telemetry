// BLE sync service payloads, schema/PROTOCOL.md §3. Pure codec; the
// transport layer moves the bytes.
import { readU16, readU32, writeU16, writeU32 } from './bytes';
import { crc16 } from './crc16';
import { decodeRecord, PROTOCOL_VERSION, Record, RECORD_SIZE, RecordDecodeError } from './record';

export const SERVICE_UUID = '6eaedc54-f770-40e3-9806-a6ccf63c8099';
export const DEVICE_INFO_CHAR_UUID = '6eaedc54-f770-40e3-0001-a6ccf63c8099';
export const CONTROL_CHAR_UUID = '6eaedc54-f770-40e3-0002-a6ccf63c8099';
export const DATA_CHAR_UUID = '6eaedc54-f770-40e3-0003-a6ccf63c8099';
export const STATUS_CHAR_UUID = '6eaedc54-f770-40e3-0004-a6ccf63c8099';

export const MANUFACTURER_ID = 0xffff;
export const ADVERTISING_SIZE = 6;
export const DEVICE_INFO_SIZE = 54;
export const STATUS_SIZE = 6;
export const CHUNK_HEADER_SIZE = 8;
export const MAX_RECORDS_PER_CHUNK = 24;
export const SESSION_ID_SIZE = 16;
export const MTU = 512;

// --- advertising (§3.1) ---

export const ADV_FLAG_TIME_SET = 0x01;
export const ADV_FLAG_DROPPED = 0x02;

export interface Advertising {
  protocolVersion: number;
  pending: number;
  batteryMv: number;
  timeSet: boolean;
  dropped: boolean;
}

/** `payload` is the manufacturer data after the 2-byte company id. */
export function decodeAdvertising(payload: Uint8Array): Advertising | null {
  if (payload.length < ADVERTISING_SIZE) return null;
  return {
    protocolVersion: payload[0],
    pending: readU16(payload, 1),
    batteryMv: readU16(payload, 3),
    timeSet: (payload[5] & ADV_FLAG_TIME_SET) !== 0,
    dropped: (payload[5] & ADV_FLAG_DROPPED) !== 0,
  };
}

/**
 * Manufacturer data as delivered by the BLE stack includes the company id
 * first. Returns null when the id is not ours.
 */
export function decodeManufacturerData(data: Uint8Array): Advertising | null {
  if (data.length < 2 + ADVERTISING_SIZE) return null;
  if (readU16(data, 0) !== MANUFACTURER_ID) return null;
  return decodeAdvertising(data.subarray(2));
}

export function encodeAdvertising(a: Advertising): Uint8Array {
  const out = new Uint8Array(ADVERTISING_SIZE);
  out[0] = a.protocolVersion;
  writeU16(out, 1, a.pending);
  writeU16(out, 3, a.batteryMv);
  out[5] = (a.timeSet ? ADV_FLAG_TIME_SET : 0) | (a.dropped ? ADV_FLAG_DROPPED : 0);
  return out;
}

// --- device info (§3.2) ---

export interface DeviceInfo {
  protocolVersion: number;
  recordSize: number;
  /** 12 lowercase hex characters. */
  deviceId: string;
  bootId: number;
  nextSeq: number;
  collectedThrough: number;
  securedThrough: number;
  uptimeS: number;
  /** 0 when the sensor's clock has never been set. */
  unixTime: number;
  dropped: number;
  batteryMv: number;
  capacity: number;
  fwVersion: string;
}

export function decodeDeviceInfo(b: Uint8Array): DeviceInfo {
  if (b.length < DEVICE_INFO_SIZE) throw new Error('device info too short');
  let deviceId = '';
  for (let i = 2; i < 8; i++) deviceId += b[i].toString(16).padStart(2, '0');
  let fw = '';
  for (let i = 38; i < 54 && b[i] !== 0; i++) fw += String.fromCharCode(b[i]);
  return {
    protocolVersion: b[0],
    recordSize: b[1],
    deviceId,
    bootId: readU16(b, 8),
    nextSeq: readU32(b, 10),
    collectedThrough: readU32(b, 14),
    securedThrough: readU32(b, 18),
    uptimeS: readU32(b, 22),
    unixTime: readU32(b, 26),
    dropped: readU32(b, 30),
    batteryMv: readU16(b, 34),
    capacity: readU16(b, 36),
    fwVersion: fw,
  };
}

export function encodeDeviceInfo(d: DeviceInfo): Uint8Array {
  const out = new Uint8Array(DEVICE_INFO_SIZE);
  out[0] = d.protocolVersion;
  out[1] = d.recordSize;
  for (let i = 0; i < 6; i++) out[2 + i] = parseInt(d.deviceId.slice(i * 2, i * 2 + 2), 16);
  writeU16(out, 8, d.bootId);
  writeU32(out, 10, d.nextSeq);
  writeU32(out, 14, d.collectedThrough);
  writeU32(out, 18, d.securedThrough);
  writeU32(out, 22, d.uptimeS);
  writeU32(out, 26, d.unixTime);
  writeU32(out, 30, d.dropped);
  writeU16(out, 34, d.batteryMv);
  writeU16(out, 36, d.capacity);
  for (let i = 0; i < 16 && i < d.fwVersion.length; i++) out[38 + i] = d.fwVersion.charCodeAt(i) & 0xff;
  return out;
}

// --- control (§3.3) ---

export const Opcode = {
  OpenSession: 0x01,
  SetTime: 0x02,
  ReadFrom: 0x03,
  AckCollected: 0x04,
  AckSecured: 0x05,
  CloseSession: 0x06,
} as const;

export type Command =
  | { opcode: typeof Opcode.OpenSession; sessionId: Uint8Array }
  | { opcode: typeof Opcode.SetTime; unixTime: number }
  | { opcode: typeof Opcode.ReadFrom; seq: number; maxRecords: number }
  | { opcode: typeof Opcode.AckCollected; throughSeq: number }
  | { opcode: typeof Opcode.AckSecured; throughSeq: number }
  | { opcode: typeof Opcode.CloseSession };

export function encodeCommand(c: Command): Uint8Array {
  switch (c.opcode) {
    case Opcode.OpenSession: {
      if (c.sessionId.length !== SESSION_ID_SIZE) throw new Error('session id must be 16 bytes');
      const out = new Uint8Array(1 + SESSION_ID_SIZE);
      out[0] = c.opcode;
      out.set(c.sessionId, 1);
      return out;
    }
    case Opcode.SetTime: {
      const out = new Uint8Array(5);
      out[0] = c.opcode;
      writeU32(out, 1, c.unixTime);
      return out;
    }
    case Opcode.ReadFrom: {
      const out = new Uint8Array(7);
      out[0] = c.opcode;
      writeU32(out, 1, c.seq);
      writeU16(out, 5, c.maxRecords);
      return out;
    }
    case Opcode.AckCollected:
    case Opcode.AckSecured: {
      const out = new Uint8Array(5);
      out[0] = c.opcode;
      writeU32(out, 1, c.throughSeq);
      return out;
    }
    case Opcode.CloseSession:
      return new Uint8Array([c.opcode]);
  }
}

export type CommandError = 'bad_length' | 'unknown_opcode';

/** Sensor-side decode, used by the fake sensor and the vector tests. */
export function decodeCommand(b: Uint8Array): Command | CommandError {
  if (b.length < 1) return 'bad_length';
  const op = b[0];
  const need: { [op: number]: number } = {
    [Opcode.OpenSession]: 1 + SESSION_ID_SIZE,
    [Opcode.SetTime]: 5,
    [Opcode.ReadFrom]: 7,
    [Opcode.AckCollected]: 5,
    [Opcode.AckSecured]: 5,
    [Opcode.CloseSession]: 1,
  };
  if (!(op in need)) return 'unknown_opcode';
  if (b.length !== need[op]) return 'bad_length';
  switch (op) {
    case Opcode.OpenSession:
      return { opcode: op, sessionId: b.slice(1, 1 + SESSION_ID_SIZE) };
    case Opcode.SetTime:
      return { opcode: op, unixTime: readU32(b, 1) };
    case Opcode.ReadFrom:
      return { opcode: op, seq: readU32(b, 1), maxRecords: readU16(b, 5) };
    case Opcode.AckCollected:
      return { opcode: op, throughSeq: readU32(b, 1) };
    case Opcode.AckSecured:
      return { opcode: op, throughSeq: readU32(b, 1) };
    default:
      return { opcode: Opcode.CloseSession };
  }
}

// --- status (§3.5) ---

export const Result = {
  Ok: 0,
  NoSession: 1,
  BadArgument: 2,
  SeqOutOfRange: 3,
  Busy: 4,
} as const;

export interface Status {
  opcode: number;
  result: number;
  seq: number;
}

export function resultName(r: number): string {
  return ['ok', 'no_session', 'bad_argument', 'seq_out_of_range', 'busy'][r] ?? `unknown(${r})`;
}

export function decodeStatus(b: Uint8Array): Status {
  if (b.length < STATUS_SIZE) throw new Error('status too short');
  return { opcode: b[0], result: b[1], seq: readU32(b, 2) };
}

export function encodeStatus(s: Status): Uint8Array {
  const out = new Uint8Array(STATUS_SIZE);
  out[0] = s.opcode;
  out[1] = s.result;
  writeU32(out, 2, s.seq);
  return out;
}

// --- data chunk (§3.4) ---

export const CHUNK_FLAG_LAST = 0x01;

export interface Chunk {
  firstSeq: number;
  count: number;
  last: boolean;
  crc: number;
  records: Record[];
}

export class ChunkError extends Error {
  constructor(message: string) {
    super(message);
    this.name = 'ChunkError';
  }
}

/** Verifies the CRC and seq continuity, and decodes every record. */
export function decodeChunk(b: Uint8Array): Chunk {
  if (b.length < CHUNK_HEADER_SIZE) throw new ChunkError('chunk too short');
  const firstSeq = readU32(b, 0);
  const count = b[4];
  const flags = b[5];
  const crc = readU16(b, 6);
  const body = b.subarray(CHUNK_HEADER_SIZE);
  if (body.length !== count * RECORD_SIZE) {
    throw new ChunkError(`chunk body is ${body.length} bytes for count ${count}`);
  }
  if (crc16(body) !== crc) throw new ChunkError(`chunk crc mismatch at seq ${firstSeq}`);
  const records: Record[] = [];
  for (let i = 0; i < count; i++) {
    let r: Record;
    try {
      r = decodeRecord(body.subarray(i * RECORD_SIZE, (i + 1) * RECORD_SIZE));
    } catch (e) {
      const code = e instanceof RecordDecodeError ? e.code : String(e);
      throw new ChunkError(`record ${i} in chunk ${firstSeq}: ${code}`);
    }
    if (r.seq !== firstSeq + i) {
      throw new ChunkError(`record seq ${r.seq} not consecutive in chunk from ${firstSeq}`);
    }
    records.push(r);
  }
  return { firstSeq, count, last: (flags & CHUNK_FLAG_LAST) !== 0, crc, records };
}

export function encodeChunk(firstSeq: number, last: boolean, encodedRecords: Uint8Array[]): Uint8Array {
  const body = new Uint8Array(encodedRecords.length * RECORD_SIZE);
  encodedRecords.forEach((r, i) => body.set(r, i * RECORD_SIZE));
  const out = new Uint8Array(CHUNK_HEADER_SIZE + body.length);
  writeU32(out, 0, firstSeq);
  out[4] = encodedRecords.length;
  out[5] = last ? CHUNK_FLAG_LAST : 0;
  writeU16(out, 6, crc16(body));
  out.set(body, CHUNK_HEADER_SIZE);
  return out;
}

export function recordsPerChunk(maxLen: number): number {
  if (maxLen <= CHUNK_HEADER_SIZE) return 0;
  return Math.min(MAX_RECORDS_PER_CHUNK, Math.floor((maxLen - CHUNK_HEADER_SIZE) / RECORD_SIZE));
}

export { PROTOCOL_VERSION };
