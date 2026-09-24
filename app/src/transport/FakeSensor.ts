// A sensor that speaks the sync protocol in memory: the phone-side twin of the
// firmware's RingStore + SyncSession. Used by tests and by FakeTransport so
// the UI can run without hardware. Behaviour follows schema/PROTOCOL.md and
// mirrors lib/telemetry-core; keep the two in step.
import {
  Advertising,
  Command,
  decodeCommand,
  DeviceInfo,
  encodeChunk,
  Opcode,
  PROTOCOL_VERSION,
  recordsPerChunk,
  Result,
  Status,
} from '../protocol/ble';
import { encodeRecord, FLAG_EPOCH_VALID, Record, RECORD_SIZE, ReadingType } from '../protocol/record';

export interface FakeSensorOptions {
  deviceId: string;
  bootId?: number;
  capacity?: number;
  fwVersion?: string;
  batteryMv?: number;
  /** Uptime in seconds at construction. */
  uptimeS?: number;
}

export class FakeSensor {
  readonly deviceId: string;
  bootId: number;
  capacity: number;
  fwVersion: string;
  batteryMv: number;
  uptimeS: number;

  private records = new Map<number, Record>();
  private nextSeq = 1;
  private tailSeq = 1;
  private collectedThrough = 0;
  private securedThrough = 0;
  private dropped = 0;
  /** Unix time at the moment the clock was set, and the uptime then. */
  private unixAtSet = 0;
  private uptimeAtSet = 0;
  private timeSet = false;

  private sessionOpen = false;
  sessionId: Uint8Array | null = null;

  /** Test hooks. */
  corruptSeqs = new Set<number>();
  commandLog: Command[] = [];

  constructor(opts: FakeSensorOptions) {
    this.deviceId = opts.deviceId;
    this.bootId = opts.bootId ?? 1;
    this.capacity = opts.capacity ?? 8192;
    this.fwVersion = opts.fwVersion ?? 'fake-0.1.0';
    this.batteryMv = opts.batteryMv ?? 0;
    this.uptimeS = opts.uptimeS ?? 0;
  }

  // --- the sensor's own life ---

  advance(seconds: number): void {
    this.uptimeS += seconds;
  }

  unixTime(): number {
    return this.timeSet ? this.unixAtSet + (this.uptimeS - this.uptimeAtSet) : 0;
  }

  /** Takes a reading, like the firmware's sampling loop. Returns the stored record. */
  sample(value: number, type: number = ReadingType.SoilResistanceOhms, quality = 0): Record {
    if (this.nextSeq - this.tailSeq >= this.capacity) {
      this.records.delete(this.tailSeq);
      this.tailSeq++;
      this.dropped++;
    }
    const now = this.unixTime();
    const r: Record = {
      version: 1,
      type,
      quality,
      flags: now ? FLAG_EPOCH_VALID : 0,
      seq: this.nextSeq,
      time: now || this.uptimeS,
      value,
      bootId: this.bootId,
    };
    this.records.set(r.seq, r);
    this.nextSeq++;
    return r;
  }

  /** Simulates a power cycle: new boot id, clock lost, session gone. */
  reboot(): void {
    this.bootId++;
    this.timeSet = false;
    this.uptimeS = 0;
    this.onDisconnect();
  }

  size(): number {
    return this.nextSeq - this.tailSeq;
  }

  pending(): number {
    const first = Math.max(this.collectedThrough + 1, this.tailSeq);
    return first < this.nextSeq ? this.nextSeq - first : 0;
  }

  cursor() {
    return {
      nextSeq: this.nextSeq,
      tailSeq: this.tailSeq,
      collectedThrough: this.collectedThrough,
      securedThrough: this.securedThrough,
      dropped: this.dropped,
    };
  }

  // --- what the radio sees ---

  advertising(): Advertising {
    return {
      protocolVersion: PROTOCOL_VERSION,
      pending: Math.min(0xffff, this.pending()),
      batteryMv: this.batteryMv,
      timeSet: this.timeSet,
      dropped: this.dropped > 0,
    };
  }

  deviceInfo(): DeviceInfo {
    return {
      protocolVersion: PROTOCOL_VERSION,
      recordSize: RECORD_SIZE,
      deviceId: this.deviceId,
      bootId: this.bootId,
      nextSeq: this.nextSeq,
      collectedThrough: this.collectedThrough,
      securedThrough: this.securedThrough,
      uptimeS: this.uptimeS,
      unixTime: this.unixTime(),
      dropped: this.dropped,
      batteryMv: this.batteryMv,
      capacity: Math.min(0xffff, this.capacity),
      fwVersion: this.fwVersion,
    };
  }

  onDisconnect(): void {
    this.sessionOpen = false;
    this.sessionId = null;
  }

  /**
   * Handles one Control write. Returns the Status and, for READ_FROM, every
   * Data chunk the firmware would stream, sized to `mtu - 3`.
   */
  handleCommand(bytes: Uint8Array, mtu: number): { status: Status; chunks: Uint8Array[] } {
    const decoded = decodeCommand(bytes);
    if (typeof decoded === 'string') {
      return { status: { opcode: bytes[0] ?? 0, result: Result.BadArgument, seq: 0 }, chunks: [] };
    }
    const cmd = decoded;
    this.commandLog.push(cmd);
    const ok = (seq: number): Status => ({ opcode: cmd.opcode, result: Result.Ok, seq });

    if (!this.sessionOpen && cmd.opcode !== Opcode.OpenSession) {
      return { status: { opcode: cmd.opcode, result: Result.NoSession, seq: 0 }, chunks: [] };
    }

    switch (cmd.opcode) {
      case Opcode.OpenSession:
        this.sessionOpen = true;
        this.sessionId = cmd.sessionId;
        return { status: ok(this.nextSeq), chunks: [] };

      case Opcode.SetTime:
        if (cmd.unixTime === 0) {
          return { status: { opcode: cmd.opcode, result: Result.BadArgument, seq: 0 }, chunks: [] };
        }
        this.timeSet = true;
        this.unixAtSet = cmd.unixTime;
        this.uptimeAtSet = this.uptimeS;
        return { status: ok(cmd.unixTime), chunks: [] };

      case Opcode.ReadFrom: {
        const from = Math.max(cmd.seq, this.tailSeq);
        return { status: ok(from), chunks: this.stream(from, cmd.maxRecords, mtu - 3) };
      }

      case Opcode.AckCollected:
        if (cmd.throughSeq >= this.nextSeq) {
          return { status: { opcode: cmd.opcode, result: Result.SeqOutOfRange, seq: this.collectedThrough }, chunks: [] };
        }
        this.collectedThrough = Math.max(this.collectedThrough, cmd.throughSeq);
        return { status: ok(this.collectedThrough), chunks: [] };

      case Opcode.AckSecured:
        if (cmd.throughSeq >= this.nextSeq) {
          return { status: { opcode: cmd.opcode, result: Result.SeqOutOfRange, seq: this.securedThrough }, chunks: [] };
        }
        if (cmd.throughSeq > this.securedThrough) {
          this.securedThrough = cmd.throughSeq;
          this.collectedThrough = Math.max(this.collectedThrough, cmd.throughSeq);
          for (let s = this.tailSeq; s <= cmd.throughSeq; s++) this.records.delete(s);
          this.tailSeq = Math.max(this.tailSeq, cmd.throughSeq + 1);
        }
        return { status: ok(this.securedThrough), chunks: [] };

      case Opcode.CloseSession:
        this.sessionOpen = false;
        this.sessionId = null;
        return { status: ok(this.nextSeq), chunks: [] };
    }
  }

  private readable(seq: number): Record | null {
    if (this.corruptSeqs.has(seq)) return null;
    return this.records.get(seq) ?? null;
  }

  /** Same packing rules as SyncSession::nextChunk: leading run of consecutive seqs. */
  private stream(from: number, maxRecords: number, maxLen: number): Uint8Array[] {
    const perChunk = recordsPerChunk(maxLen);
    if (perChunk === 0) return [];
    const chunks: Uint8Array[] = [];
    let seq = from;
    let remaining = maxRecords === 0 ? Infinity : maxRecords;

    for (;;) {
      // Skip unreadable slots, as the firmware's read() does.
      while (seq < this.nextSeq && this.readable(seq) === null) seq++;
      const packed: Uint8Array[] = [];
      let first = seq;
      while (seq < this.nextSeq && packed.length < perChunk && packed.length < remaining) {
        const r = this.readable(seq);
        if (r === null) break; // gap ends the chunk
        packed.push(encodeRecord(r));
        seq++;
      }
      remaining -= packed.length;
      // More to stream if records remain within the window.
      let probe = seq;
      while (probe < this.nextSeq && this.readable(probe) === null) probe++;
      const more = remaining > 0 && probe < this.nextSeq;
      chunks.push(encodeChunk(first, !more, packed));
      if (!more) return chunks;
    }
  }
}
