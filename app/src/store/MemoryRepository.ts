import { DeviceRow, ReadingCounts, Repository, SessionRow, StoredReading } from './Repository';

export function emptyDevice(deviceId: string): DeviceRow {
  return {
    deviceId,
    name: null,
    lastSeenAt: null,
    lastBootId: null,
    sensorNextSeq: null,
    sensorCollectedThrough: 0,
    sensorSecuredThrough: 0,
    serverAckedThrough: 0,
    pendingOnSensor: null,
    fwVersion: null,
  };
}

export class MemoryRepository implements Repository {
  settings = new Map<string, string>();
  devices = new Map<string, DeviceRow>();
  readings = new Map<string, Map<number, StoredReading>>();
  sessions = new Map<string, SessionRow>();

  async init(): Promise<void> {}

  async getSetting(key: string): Promise<string | null> {
    return this.settings.get(key) ?? null;
  }

  async setSetting(key: string, value: string): Promise<void> {
    this.settings.set(key, value);
  }

  async upsertDevice(row: Partial<DeviceRow> & { deviceId: string }): Promise<void> {
    const prev = this.devices.get(row.deviceId) ?? emptyDevice(row.deviceId);
    this.devices.set(row.deviceId, { ...prev, ...row });
  }

  async getDevice(deviceId: string): Promise<DeviceRow | null> {
    return this.devices.get(deviceId) ?? null;
  }

  async listDevices(): Promise<DeviceRow[]> {
    return [...this.devices.values()].sort((a, b) => (b.lastSeenAt ?? 0) - (a.lastSeenAt ?? 0));
  }

  async insertReadings(rows: StoredReading[]): Promise<number> {
    let n = 0;
    for (const r of rows) {
      let dev = this.readings.get(r.deviceId);
      if (!dev) {
        dev = new Map();
        this.readings.set(r.deviceId, dev);
      }
      if (dev.has(r.seq)) continue;
      dev.set(r.seq, { ...r });
      n++;
    }
    return n;
  }

  async maxSeq(deviceId: string): Promise<number> {
    let max = 0;
    for (const seq of this.readings.get(deviceId)?.keys() ?? []) if (seq > max) max = seq;
    return max;
  }

  async countReadings(deviceId: string): Promise<ReadingCounts> {
    let collected = 0;
    let uploaded = 0;
    for (const r of this.readings.get(deviceId)?.values() ?? []) {
      collected++;
      if (r.uploadedAt !== null) uploaded++;
    }
    return { collected, uploaded };
  }

  async listReadings(deviceId: string, limit: number): Promise<StoredReading[]> {
    return [...(this.readings.get(deviceId)?.values() ?? [])].sort((a, b) => b.seq - a.seq).slice(0, limit);
  }

  async listUnuploaded(deviceId: string, sessionId: string, limit: number): Promise<StoredReading[]> {
    return [...(this.readings.get(deviceId)?.values() ?? [])]
      .filter((r) => r.uploadedAt === null && r.rejectedReason === null && r.sessionId === sessionId)
      .sort((a, b) => a.seq - b.seq)
      .slice(0, limit);
  }

  async sessionsWithUnuploaded(deviceId: string): Promise<string[]> {
    const firstSeq = new Map<string, number>();
    for (const r of this.readings.get(deviceId)?.values() ?? []) {
      if (r.uploadedAt !== null || r.rejectedReason !== null) continue;
      const prev = firstSeq.get(r.sessionId);
      if (prev === undefined || r.seq < prev) firstSeq.set(r.sessionId, r.seq);
    }
    return [...firstSeq.entries()].sort((a, b) => a[1] - b[1]).map(([id]) => id);
  }

  async devicesWithUnuploaded(): Promise<string[]> {
    const out: string[] = [];
    for (const [dev, rows] of this.readings) {
      for (const r of rows.values()) {
        if (r.uploadedAt === null && r.rejectedReason === null) {
          out.push(dev);
          break;
        }
      }
    }
    return out.sort();
  }

  async markUploaded(deviceId: string, seqs: number[], batchId: string, uploadedAt: number): Promise<void> {
    const dev = this.readings.get(deviceId);
    if (!dev) return;
    for (const s of seqs) {
      const r = dev.get(s);
      if (r) {
        r.uploadedAt = uploadedAt;
        r.batchId = batchId;
      }
    }
  }

  async markRejected(deviceId: string, seqs: number[], reason: string): Promise<void> {
    const dev = this.readings.get(deviceId);
    if (!dev) return;
    for (const s of seqs) {
      const r = dev.get(s);
      if (r) r.rejectedReason = reason;
    }
  }

  async uploadedSeqsAbove(deviceId: string, from: number): Promise<number[]> {
    return [...(this.readings.get(deviceId)?.values() ?? [])]
      .filter((r) => r.uploadedAt !== null && r.seq > from)
      .map((r) => r.seq)
      .sort((a, b) => a - b);
  }

  async insertSession(row: SessionRow): Promise<void> {
    this.sessions.set(row.sessionId, { ...row });
  }

  async updateSession(row: SessionRow): Promise<void> {
    this.sessions.set(row.sessionId, { ...row });
  }

  async getSession(sessionId: string): Promise<SessionRow | null> {
    return this.sessions.get(sessionId) ?? null;
  }

  async listSessions(limit: number): Promise<SessionRow[]> {
    return [...this.sessions.values()].sort((a, b) => b.startedAt - a.startedAt).slice(0, limit);
  }
}
