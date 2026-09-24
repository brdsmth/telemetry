// One visit to one sensor: connect, open a session, set its clock, pass on
// any server acks, pull everything the phone lacks, store it, ack collected.
// Pure orchestration over the transport and repository interfaces.
import { Chunk, ChunkError, decodeChunk, Opcode, PROTOCOL_VERSION, Result, resultName, Status } from '../protocol/ble';
import { toHex } from '../protocol/bytes';
import { encodeRecord, epochValid, Record } from '../protocol/record';
import { Repository, SessionRow, StoredReading, TimeSource } from '../store/Repository';
import { SensorLink, SensorTransport } from '../transport/SensorTransport';
import { SyncDeps, uuidToBytes } from './deps';

export interface CollectionOptions {
  /** Records requested per READ_FROM. Default 240, ten full chunks. */
  windowSize?: number;
  /** Milliseconds to wait for the next chunk before giving up on a window. */
  chunkTimeoutMs?: number;
  /** Skip SET_TIME (tests). */
  setTime?: boolean;
}

export interface CollectionResult {
  deviceId: string;
  sessionId: string;
  /** Sensor cursors as read at connect. */
  bootId: number;
  fromSeq: number;
  /** Highest seq stored during this visit, 0 if none. */
  throughSeq: number;
  pulled: number;
  newlyStored: number;
  gaps: number;
  securedAckSent: number | null;
  pendingAfter: number;
}

export class CollectionError extends Error {
  constructor(message: string, public readonly result?: CollectionResult) {
    super(message);
    this.name = 'CollectionError';
  }
}

export class CollectionService {
  constructor(
    private readonly transport: SensorTransport,
    private readonly repo: Repository,
    private readonly deps: SyncDeps,
  ) {}

  async collect(peripheralId: string, opts: CollectionOptions = {}): Promise<CollectionResult> {
    const windowSize = opts.windowSize ?? 240;
    const chunkTimeoutMs = opts.chunkTimeoutMs ?? 10_000;

    const link = await this.transport.connect(peripheralId);
    const startedAt = this.deps.now();
    let session: SessionRow | null = null;
    try {
      const info = await link.readDeviceInfo();
      if (info.protocolVersion !== PROTOCOL_VERSION) {
        throw new CollectionError(`sensor speaks protocol v${info.protocolVersion}, app speaks v${PROTOCOL_VERSION}`);
      }
      const deviceId = info.deviceId;
      const sessionId = this.deps.uuid();
      const phoneTimeAtConnect = this.deps.now();
      session = {
        sessionId,
        deviceId,
        startedAt,
        endedAt: null,
        phoneTimeAtConnect,
        sensorUptimeAtConnect: info.uptimeS,
        sensorBootIdAtConnect: info.bootId,
        recordsPulled: 0,
        gaps: 0,
        error: null,
      };
      await this.repo.insertSession(session);
      await this.repo.upsertDevice({
        deviceId,
        lastSeenAt: phoneTimeAtConnect,
        lastBootId: info.bootId,
        sensorNextSeq: info.nextSeq,
        sensorCollectedThrough: info.collectedThrough,
        sensorSecuredThrough: info.securedThrough,
        fwVersion: info.fwVersion,
      });

      await this.expectOk(link.command({ opcode: Opcode.OpenSession, sessionId: uuidToBytes(sessionId) }), 'open session');

      if (opts.setTime !== false) {
        const s = await link.command({ opcode: Opcode.SetTime, unixTime: this.deps.now() });
        if (s.result !== Result.Ok) this.deps.log(`set time refused: ${resultName(s.result)}`);
      }

      // Pass on what the server has confirmed since the last visit.
      let securedAckSent: number | null = null;
      const device = await this.repo.getDevice(deviceId);
      const serverAcked = device?.serverAckedThrough ?? 0;
      if (serverAcked > info.securedThrough) {
        const s = await link.command({ opcode: Opcode.AckSecured, throughSeq: Math.min(serverAcked, info.nextSeq - 1) });
        if (s.result === Result.Ok) {
          securedAckSent = s.seq;
          await this.repo.upsertDevice({ deviceId, sensorSecuredThrough: s.seq });
        } else {
          this.deps.log(`secured ack refused: ${resultName(s.result)}`);
        }
      }

      // Pull from whichever side knows less: the sensor's collected cursor or
      // what this phone actually holds, so a wiped phone or a reset sensor
      // both recover.
      const phoneMax = await this.repo.maxSeq(deviceId);
      let from = Math.min(info.collectedThrough, phoneMax) + 1;
      const fromSeq = from;
      let pulled = 0;
      let newlyStored = 0;
      let gaps = 0;
      let through = 0;

      for (;;) {
        const { records, lastChunkSeen, gapsInWindow } = await this.pullWindow(link, from, windowSize, chunkTimeoutMs);
        gaps += gapsInWindow;
        if (records.length > 0) {
          const rows = records.map((r) => this.toStored(r, deviceId, sessionId, info.bootId, phoneTimeAtConnect, info.uptimeS));
          newlyStored += await this.repo.insertReadings(rows);
          pulled += records.length;
          through = records[records.length - 1].seq;
          from = through + 1;
        }
        // The sensor marks the final chunk of each window; a window that came
        // back short or empty means the sensor has nothing more right now.
        if (!lastChunkSeen || records.length < windowSize) break;
      }

      if (through > 0) {
        await this.expectOk(link.command({ opcode: Opcode.AckCollected, throughSeq: through }), 'ack collected');
        await this.repo.upsertDevice({ deviceId, sensorCollectedThrough: through });
      }

      const after = await link.readDeviceInfo();
      const pendingAfter = Math.max(0, after.nextSeq - 1 - Math.max(after.collectedThrough, after.securedThrough));
      await this.repo.upsertDevice({
        deviceId,
        sensorNextSeq: after.nextSeq,
        sensorCollectedThrough: after.collectedThrough,
        sensorSecuredThrough: after.securedThrough,
        pendingOnSensor: pendingAfter,
      });

      await link.command({ opcode: Opcode.CloseSession });

      session = { ...session, endedAt: this.deps.now(), recordsPulled: pulled, gaps };
      await this.repo.updateSession(session);

      const result: CollectionResult = {
        deviceId, sessionId, bootId: info.bootId, fromSeq, throughSeq: through,
        pulled, newlyStored, gaps, securedAckSent, pendingAfter,
      };
      this.deps.log(`collected ${pulled} from ${deviceId} (${fromSeq}..${through}), ${gaps} gaps, pending ${pendingAfter}`);
      return result;
    } catch (e) {
      if (session) {
        await this.repo.updateSession({ ...session, endedAt: this.deps.now(), error: String(e) });
      }
      throw e;
    } finally {
      await link.disconnect().catch(() => undefined);
    }
  }

  private async expectOk(p: Promise<Status>, what: string): Promise<Status> {
    const s = await p;
    if (s.result !== Result.Ok) throw new CollectionError(`${what} failed: ${resultName(s.result)}`);
    return s;
  }

  /** One READ_FROM and the chunks that follow until one carries `last`. */
  private pullWindow(
    link: SensorLink,
    from: number,
    maxRecords: number,
    timeoutMs: number,
  ): Promise<{ records: Record[]; lastChunkSeen: boolean; gapsInWindow: number }> {
    return new Promise((resolve, reject) => {
      const records: Record[] = [];
      let expected: number | null = null;
      let gaps = 0;
      let timer: ReturnType<typeof setTimeout> | null = null;
      const arm = () => {
        if (timer) clearTimeout(timer);
        timer = setTimeout(() => {
          cleanup();
          resolve({ records, lastChunkSeen: false, gapsInWindow: gaps });
        }, timeoutMs);
      };
      const unsubChunk = link.onChunk((bytes) => {
        let chunk: Chunk;
        try {
          chunk = decodeChunk(bytes);
        } catch (e) {
          cleanup();
          reject(e instanceof ChunkError ? e : new ChunkError(String(e)));
          return;
        }
        if (expected !== null && chunk.count > 0 && chunk.firstSeq !== expected) gaps++;
        if (expected === null && chunk.count > 0 && chunk.firstSeq !== from) gaps++;
        records.push(...chunk.records);
        expected = chunk.firstSeq + chunk.count;
        if (chunk.last) {
          cleanup();
          resolve({ records, lastChunkSeen: true, gapsInWindow: gaps });
        } else {
          arm();
        }
      });
      const unsubDisc = link.onDisconnect((reason) => {
        cleanup();
        reject(new CollectionError(`disconnected while pulling: ${reason}`));
      });
      const cleanup = () => {
        if (timer) clearTimeout(timer);
        unsubChunk();
        unsubDisc();
      };
      arm();
      link.command({ opcode: Opcode.ReadFrom, seq: from, maxRecords }).then(
        (s) => {
          if (s.result !== Result.Ok) {
            cleanup();
            reject(new CollectionError(`read from ${from} failed: ${resultName(s.result)}`));
          }
        },
        (e) => {
          cleanup();
          reject(e);
        },
      );
    });
  }

  private toStored(
    r: Record,
    deviceId: string,
    sessionId: string,
    bootIdAtConnect: number,
    phoneTimeAtConnect: number,
    sensorUptimeAtConnect: number,
  ): StoredReading {
    let recordedAt: number | null = null;
    let timeSource: TimeSource = 'unknown';
    if (epochValid(r)) {
      recordedAt = r.time;
      timeSource = 'sensor';
    } else if (r.bootId === bootIdAtConnect) {
      recordedAt = phoneTimeAtConnect - sensorUptimeAtConnect + r.time;
      timeSource = 'phone_backfill';
    }
    return {
      deviceId,
      seq: r.seq,
      type: r.type,
      quality: r.quality,
      flags: r.flags,
      rawTime: r.time,
      bootId: r.bootId,
      value: r.value,
      raw: toHex(encodeRecord(r)),
      recordedAt,
      timeSource,
      sessionId,
      collectedAt: this.deps.now(),
      uploadedAt: null,
      batchId: null,
      rejectedReason: null,
    };
  }
}
