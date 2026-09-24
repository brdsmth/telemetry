// The fake sensor must behave like the firmware's ring store and sync session
// so tests against it mean something. These mirror sensor/test/test_sync_session.
import { decodeChunk, encodeCommand, Opcode, Result } from '../../protocol/ble';
import { FakeSensor } from '../FakeSensor';

const sid = new Uint8Array(16).map((_, i) => i);
const open = encodeCommand({ opcode: Opcode.OpenSession, sessionId: sid });

function seeded(n: number, opts: Partial<ConstructorParameters<typeof FakeSensor>[0]> = {}) {
  const s = new FakeSensor({ deviceId: 'aabbccddeeff', ...opts });
  for (let i = 0; i < n; i++) {
    s.advance(60);
    s.sample(1000 + i);
  }
  return s;
}

function read(s: FakeSensor, from: number, max = 0, mtu = 512) {
  const { status, chunks } = s.handleCommand(encodeCommand({ opcode: Opcode.ReadFrom, seq: from, maxRecords: max }), mtu);
  return { status, chunks: chunks.map(decodeChunk) };
}

test('commands before open are refused', () => {
  const s = seeded(3);
  expect(read(s, 1).status.result).toBe(Result.NoSession);
  s.handleCommand(open, 512);
  expect(read(s, 1).status.result).toBe(Result.Ok);
});

test('malformed commands answer bad_argument and keep the session', () => {
  const s = seeded(1);
  s.handleCommand(open, 512);
  const { status } = s.handleCommand(new Uint8Array([0x02, 0x01]), 512);
  expect(status).toEqual({ opcode: 0x02, result: Result.BadArgument, seq: 0 });
  expect(read(s, 1).status.result).toBe(Result.Ok);
});

test('streams full chunks then a last chunk', () => {
  const s = seeded(30);
  s.handleCommand(open, 512);
  const { status, chunks } = read(s, 1);
  expect(status.seq).toBe(1);
  expect(chunks.map((c) => [c.firstSeq, c.count, c.last])).toEqual([
    [1, 24, false],
    [25, 6, true],
  ]);
  expect(chunks[0].records[0].value).toBe(1000);
});

test('window limits records and a small mtu packs fewer per chunk', () => {
  const s = seeded(30);
  s.handleCommand(open, 512);
  expect(read(s, 3, 5).chunks.map((c) => [c.firstSeq, c.count, c.last])).toEqual([[3, 5, true]]);
  expect(read(s, 1, 0, 71).chunks.slice(0, 2).map((c) => c.count)).toEqual([3, 3]); // (71 - 3 - 8) / 20
});

test('read below tail clamps, beyond end yields an empty last chunk', () => {
  const s = seeded(6, { capacity: 4 });
  s.handleCommand(open, 512);
  expect(s.cursor()).toMatchObject({ tailSeq: 3, nextSeq: 7, dropped: 2 });
  const low = read(s, 1);
  expect(low.status.seq).toBe(3);
  expect(low.chunks[0].firstSeq).toBe(3);
  const high = read(s, 99);
  expect(high.chunks).toHaveLength(1);
  expect(high.chunks[0]).toMatchObject({ count: 0, last: true });
});

test('a corrupt slot becomes a visible gap between chunks', () => {
  const s = seeded(10);
  s.corruptSeqs.add(5);
  s.handleCommand(open, 512);
  const { chunks } = read(s, 1);
  expect(chunks.map((c) => [c.firstSeq, c.count, c.last])).toEqual([
    [1, 4, false],
    [6, 5, true],
  ]);
});

test('collected ack frees nothing, secured ack frees slots and raises collected', () => {
  const s = seeded(10);
  s.handleCommand(open, 512);
  let r = s.handleCommand(encodeCommand({ opcode: Opcode.AckCollected, throughSeq: 7 }), 512).status;
  expect(r).toEqual({ opcode: Opcode.AckCollected, result: Result.Ok, seq: 7 });
  expect(s.pending()).toBe(3);
  expect(s.size()).toBe(10);
  r = s.handleCommand(encodeCommand({ opcode: Opcode.AckSecured, throughSeq: 4 }), 512).status;
  expect(r.seq).toBe(4);
  expect(s.cursor()).toMatchObject({ tailSeq: 5, securedThrough: 4, collectedThrough: 7 });
  expect(s.size()).toBe(6);
  const future = s.handleCommand(encodeCommand({ opcode: Opcode.AckSecured, throughSeq: 11 }), 512).status;
  expect(future.result).toBe(Result.SeqOutOfRange);
});

test('set time makes later records carry epoch and advertising reflects state', () => {
  const s = seeded(2);
  expect(s.advertising()).toMatchObject({ pending: 2, timeSet: false, dropped: false });
  s.handleCommand(open, 512);
  s.handleCommand(encodeCommand({ opcode: Opcode.SetTime, unixTime: 1_790_121_600 }), 512);
  s.advance(60);
  const r = s.sample(5);
  expect(r.flags & 1).toBe(1);
  expect(r.time).toBe(1_790_121_660);
  expect(s.deviceInfo().unixTime).toBe(1_790_121_660);
  expect(s.advertising().timeSet).toBe(true);
  s.reboot();
  expect(s.deviceInfo()).toMatchObject({ bootId: 2, unixTime: 0 });
});

test('disconnect drops the session', () => {
  const s = seeded(1);
  s.handleCommand(open, 512);
  s.onDisconnect();
  expect(read(s, 1).status.result).toBe(Result.NoSession);
});
