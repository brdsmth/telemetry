import fs from 'fs';
import path from 'path';

import { fromBase64, fromHex, toBase64, toHex } from '../bytes';
import { crc16 } from '../crc16';
import { decodeRecord, encodeRecord, qualityName, readingTypeName, RECORD_SIZE, RECORD_VERSION, RecordDecodeError } from '../record';

type Vectors = {
  format: string;
  version: number;
  size: number;
  crc: { check_input: string; check: string };
  valid: { name: string; hex: string; record: { version: number; type: number; quality: number; flags: number; seq: number; time: number; value: number; boot_id: number } }[];
  invalid: { name: string; hex: string; error: string }[];
};

const vectors: Vectors = JSON.parse(
  fs.readFileSync(path.join(__dirname, '..', '..', '..', '..', 'schema', 'vectors', 'records.json'), 'utf8'),
);

describe('record codec against schema/vectors/records.json', () => {
  test('vector file is the record format we implement', () => {
    expect(vectors.format).toBe('record');
    expect(vectors.version).toBe(RECORD_VERSION);
    expect(vectors.size).toBe(RECORD_SIZE);
  });

  test('crc check value', () => {
    const input = new TextEncoder().encode(vectors.crc.check_input);
    expect(crc16(input).toString(16).padStart(4, '0')).toBe(vectors.crc.check);
  });

  test.each(vectors.valid.map((v) => [v.name, v] as const))('decodes %s', (_name, v) => {
    const r = decodeRecord(fromHex(v.hex));
    expect(r).toEqual({
      version: v.record.version,
      type: v.record.type,
      quality: v.record.quality,
      flags: v.record.flags,
      seq: v.record.seq,
      time: v.record.time,
      value: v.record.value,
      bootId: v.record.boot_id,
    });
  });

  test.each(vectors.valid.map((v) => [v.name, v] as const))('encodes %s byte for byte', (_name, v) => {
    const encoded = encodeRecord({
      version: v.record.version,
      type: v.record.type,
      quality: v.record.quality,
      flags: v.record.flags,
      seq: v.record.seq,
      time: v.record.time,
      value: v.record.value,
      bootId: v.record.boot_id,
    });
    expect(toHex(encoded)).toBe(v.hex);
  });

  test.each(vectors.invalid.map((v) => [v.name, v] as const))('rejects %s', (_name, v) => {
    expect(() => decodeRecord(fromHex(v.hex))).toThrow(RecordDecodeError);
    try {
      decodeRecord(fromHex(v.hex));
    } catch (e) {
      expect((e as RecordDecodeError).code).toBe(v.error);
    }
  });

  test('names', () => {
    expect(readingTypeName(1)).toBe('soil_resistance_ohms');
    expect(readingTypeName(250)).toBe('unknown');
    expect(qualityName(3)).toBe('open');
    expect(qualityName(9)).toBe('unknown');
  });
});

describe('base64 helpers', () => {
  test('round trips every vector and matches the standard alphabet', () => {
    for (const v of vectors.valid) {
      const bytes = fromHex(v.hex);
      const b64 = toBase64(bytes);
      expect(b64).toBe(Buffer.from(bytes).toString('base64'));
      expect(toHex(fromBase64(b64))).toBe(v.hex);
    }
  });

  test('handles lengths that need one or two padding characters', () => {
    for (const len of [0, 1, 2, 3, 4, 5, 19, 20, 21]) {
      const bytes = new Uint8Array(len).map((_, i) => (i * 37) & 0xff);
      expect(toBase64(bytes)).toBe(Buffer.from(bytes).toString('base64'));
      expect(Array.from(fromBase64(toBase64(bytes)))).toEqual(Array.from(bytes));
    }
  });
});
