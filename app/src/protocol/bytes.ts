// Byte helpers shared by the codecs. Little-endian throughout, matching
// schema/PROTOCOL.md. No Node Buffer: React Native does not have it.

export function readU16(b: Uint8Array, offset: number): number {
  return b[offset] | (b[offset + 1] << 8);
}

export function readU32(b: Uint8Array, offset: number): number {
  // >>> 0 keeps the result unsigned.
  return (b[offset] | (b[offset + 1] << 8) | (b[offset + 2] << 16) | (b[offset + 3] << 24)) >>> 0;
}

export function readF32(b: Uint8Array, offset: number): number {
  return new DataView(b.buffer, b.byteOffset + offset, 4).getFloat32(0, true);
}

export function writeU16(b: Uint8Array, offset: number, v: number): void {
  b[offset] = v & 0xff;
  b[offset + 1] = (v >>> 8) & 0xff;
}

export function writeU32(b: Uint8Array, offset: number, v: number): void {
  b[offset] = v & 0xff;
  b[offset + 1] = (v >>> 8) & 0xff;
  b[offset + 2] = (v >>> 16) & 0xff;
  b[offset + 3] = (v >>> 24) & 0xff;
}

export function writeF32(b: Uint8Array, offset: number, v: number): void {
  new DataView(b.buffer, b.byteOffset + offset, 4).setFloat32(0, v, true);
}

export function toHex(b: Uint8Array): string {
  let s = '';
  for (const x of b) s += x.toString(16).padStart(2, '0');
  return s;
}

export function fromHex(hex: string): Uint8Array {
  if (hex.length % 2 !== 0) throw new Error('odd hex length');
  const out = new Uint8Array(hex.length / 2);
  for (let i = 0; i < out.length; i++) out[i] = parseInt(hex.slice(i * 2, i * 2 + 2), 16);
  return out;
}

const B64 = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
const B64_INDEX: Record<string, number> = {};
for (let i = 0; i < B64.length; i++) B64_INDEX[B64[i]] = i;

// react-native-ble-plx hands characteristic values over as base64 and takes
// base64 back, so the codecs work in bytes and convert at the edge.
export function toBase64(b: Uint8Array): string {
  let out = '';
  for (let i = 0; i < b.length; i += 3) {
    const n = (b[i] << 16) | ((i + 1 < b.length ? b[i + 1] : 0) << 8) | (i + 2 < b.length ? b[i + 2] : 0);
    out += B64[(n >>> 18) & 63] + B64[(n >>> 12) & 63];
    out += i + 1 < b.length ? B64[(n >>> 6) & 63] : '=';
    out += i + 2 < b.length ? B64[n & 63] : '=';
  }
  return out;
}

export function fromBase64(s: string): Uint8Array {
  const clean = s.replace(/[^A-Za-z0-9+/]/g, '');
  const out = new Uint8Array(Math.floor((clean.length * 3) / 4));
  let o = 0;
  for (let i = 0; i + 1 < clean.length; i += 4) {
    const c0 = B64_INDEX[clean[i]];
    const c1 = B64_INDEX[clean[i + 1]];
    const c2 = i + 2 < clean.length ? B64_INDEX[clean[i + 2]] : 0;
    const c3 = i + 3 < clean.length ? B64_INDEX[clean[i + 3]] : 0;
    if (c0 === undefined || c1 === undefined || c2 === undefined || c3 === undefined) {
      throw new Error('bad base64');
    }
    const n = (c0 << 18) | (c1 << 12) | (c2 << 6) | c3;
    out[o++] = (n >>> 16) & 0xff;
    if (i + 2 < clean.length) out[o++] = (n >>> 8) & 0xff;
    if (i + 3 < clean.length) out[o++] = n & 0xff;
  }
  return out.subarray(0, o);
}

export function concat(parts: Uint8Array[]): Uint8Array {
  let n = 0;
  for (const p of parts) n += p.length;
  const out = new Uint8Array(n);
  let o = 0;
  for (const p of parts) {
    out.set(p, o);
    o += p.length;
  }
  return out;
}
