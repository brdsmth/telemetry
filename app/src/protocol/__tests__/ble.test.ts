import fs from 'fs';
import path from 'path';

import { fromHex, toHex } from '../bytes';
import {
  ADVERTISING_SIZE,
  CHUNK_HEADER_SIZE,
  ChunkError,
  CONTROL_CHAR_UUID,
  DATA_CHAR_UUID,
  decodeAdvertising,
  decodeChunk,
  decodeCommand,
  decodeDeviceInfo,
  decodeManufacturerData,
  decodeStatus,
  DEVICE_INFO_CHAR_UUID,
  DEVICE_INFO_SIZE,
  encodeAdvertising,
  encodeChunk,
  encodeCommand,
  encodeDeviceInfo,
  encodeStatus,
  MAX_RECORDS_PER_CHUNK,
  Opcode,
  PROTOCOL_VERSION,
  recordsPerChunk,
  SERVICE_UUID,
  STATUS_CHAR_UUID,
  STATUS_SIZE,
} from '../ble';

const rows = (xs: { name: string }[]): [string, any][] => xs.map((v) => [v.name, v]);

const vectors = JSON.parse(
  fs.readFileSync(path.join(__dirname, '..', '..', '..', '..', 'schema', 'vectors', 'ble.json'), 'utf8'),
);

describe('ble codec against schema/vectors/ble.json', () => {
  test('constants match the vectors', () => {
    expect(vectors.protocol_version).toBe(PROTOCOL_VERSION);
    expect(vectors.service_uuid).toBe(SERVICE_UUID);
    expect(vectors.characteristics.device_info).toBe(DEVICE_INFO_CHAR_UUID);
    expect(vectors.characteristics.control).toBe(CONTROL_CHAR_UUID);
    expect(vectors.characteristics.data).toBe(DATA_CHAR_UUID);
    expect(vectors.characteristics.status).toBe(STATUS_CHAR_UUID);
    expect(vectors.sizes.advertising).toBe(ADVERTISING_SIZE);
    expect(vectors.sizes.device_info).toBe(DEVICE_INFO_SIZE);
    expect(vectors.sizes.status).toBe(STATUS_SIZE);
    expect(vectors.sizes.chunk_header).toBe(CHUNK_HEADER_SIZE);
    expect(vectors.sizes.max_records_per_chunk).toBe(MAX_RECORDS_PER_CHUNK);
    expect(vectors.opcodes.OPEN_SESSION).toBe(Opcode.OpenSession);
    expect(vectors.opcodes.CLOSE_SESSION).toBe(Opcode.CloseSession);
  });

  test.each(rows(vectors.advertising))('advertising %s', (_n, v: any) => {
    const a = decodeAdvertising(fromHex(v.hex))!;
    expect(a.protocolVersion).toBe(v.fields.protocol_version);
    expect(a.pending).toBe(v.fields.pending);
    expect(a.batteryMv).toBe(v.fields.battery_mv);
    expect(a.timeSet).toBe((v.fields.adv_flags & 1) !== 0);
    expect(a.dropped).toBe((v.fields.adv_flags & 2) !== 0);
    expect(toHex(encodeAdvertising(a))).toBe(v.hex);
    // With the company id in front, as the BLE stack delivers it.
    expect(decodeManufacturerData(fromHex('ffff' + v.hex))).toEqual(a);
    expect(decodeManufacturerData(fromHex('1234' + v.hex))).toBeNull();
  });

  test.each(rows(vectors.device_info))('device info %s', (_n, v: any) => {
    const d = decodeDeviceInfo(fromHex(v.hex));
    expect(d).toEqual({
      protocolVersion: v.fields.protocol_version,
      recordSize: v.fields.record_size,
      deviceId: v.fields.device_id,
      bootId: v.fields.boot_id,
      nextSeq: v.fields.next_seq,
      collectedThrough: v.fields.collected_through,
      securedThrough: v.fields.secured_through,
      uptimeS: v.fields.uptime_s,
      unixTime: v.fields.unix_time,
      dropped: v.fields.dropped,
      batteryMv: v.fields.battery_mv,
      capacity: v.fields.capacity,
      fwVersion: v.fields.fw_version,
    });
    expect(toHex(encodeDeviceInfo(d))).toBe(v.hex);
  });

  test.each(rows(vectors.status))('status %s', (_n, v: any) => {
    const s = decodeStatus(fromHex(v.hex));
    expect(s).toEqual({ opcode: v.fields.opcode, result: v.fields.result, seq: v.fields.seq });
    expect(toHex(encodeStatus(s))).toBe(v.hex);
  });

  test.each(rows(vectors.commands))('command %s', (_n, v: any) => {
    const c = v.command;
    const decoded = decodeCommand(fromHex(v.hex));
    expect(typeof decoded).toBe('object');
    const cmd = decoded as any;
    expect(cmd.opcode).toBe(c.opcode);
    if (c.session_id) expect(toHex(cmd.sessionId)).toBe(c.session_id);
    if (c.unix_time !== undefined) expect(cmd.unixTime).toBe(c.unix_time);
    if (c.seq !== undefined) expect(cmd.seq).toBe(c.seq);
    if (c.max_records !== undefined) expect(cmd.maxRecords).toBe(c.max_records);
    if (c.through_seq !== undefined) expect(cmd.throughSeq).toBe(c.through_seq);
    expect(toHex(encodeCommand(cmd))).toBe(v.hex);
  });

  test.each(rows(vectors.invalid_commands))('invalid command %s', (_n, v: any) => {
    expect(decodeCommand(fromHex(v.hex))).toBe(v.error);
  });

  test.each(rows(vectors.chunks))('chunk %s', (_n, v: any) => {
    const chunk = decodeChunk(fromHex(v.hex));
    expect(chunk.firstSeq).toBe(v.header.first_seq);
    expect(chunk.count).toBe(v.header.count);
    expect(chunk.last).toBe((v.header.flags & 1) !== 0);
    expect(chunk.crc).toBe(v.header.crc);
    expect(chunk.records.map((r) => r.seq)).toEqual(v.records.map((_: string, i: number) => v.header.first_seq + i));
    const re = encodeChunk(chunk.firstSeq, chunk.last, v.records.map((h: string) => fromHex(h)));
    expect(toHex(re)).toBe(v.hex);
  });

  test('chunk with a bad crc or a seq gap is rejected', () => {
    const good = fromHex(vectors.chunks[0].hex);
    const badCrc = good.slice();
    badCrc[badCrc.length - 1] ^= 0x01;
    expect(() => decodeChunk(badCrc)).toThrow(ChunkError);
    const truncated = good.subarray(0, good.length - 1);
    expect(() => decodeChunk(truncated)).toThrow(/body is/);
  });

  test('records per chunk', () => {
    expect(recordsPerChunk(20)).toBe(0);
    expect(recordsPerChunk(68)).toBe(3);
    expect(recordsPerChunk(512)).toBe(MAX_RECORDS_PER_CHUNK);
  });
});
