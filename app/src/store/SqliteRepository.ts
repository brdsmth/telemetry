// Repository on expo-sqlite. One database file, four tables. Schema changes
// go through `migrate()` with a user_version bump.
import * as SQLite from 'expo-sqlite';

import { DeviceRow, ReadingCounts, Repository, SessionRow, StoredReading, TimeSource } from './Repository';

const SCHEMA_VERSION = 1;

const SCHEMA = `
CREATE TABLE IF NOT EXISTS settings (
  key   TEXT PRIMARY KEY,
  value TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS devices (
  device_id                TEXT PRIMARY KEY,
  name                     TEXT,
  last_seen_at             INTEGER,
  last_boot_id             INTEGER,
  sensor_next_seq          INTEGER,
  sensor_collected_through INTEGER NOT NULL DEFAULT 0,
  sensor_secured_through   INTEGER NOT NULL DEFAULT 0,
  server_acked_through     INTEGER NOT NULL DEFAULT 0,
  pending_on_sensor        INTEGER,
  fw_version               TEXT
);
CREATE TABLE IF NOT EXISTS readings (
  device_id       TEXT    NOT NULL,
  seq             INTEGER NOT NULL,
  type            INTEGER NOT NULL,
  quality         INTEGER NOT NULL,
  flags           INTEGER NOT NULL,
  raw_time        INTEGER NOT NULL,
  boot_id         INTEGER NOT NULL,
  value           REAL    NOT NULL,
  raw             TEXT    NOT NULL,
  recorded_at     INTEGER,
  time_source     TEXT    NOT NULL,
  session_id      TEXT    NOT NULL,
  collected_at    INTEGER NOT NULL,
  uploaded_at     INTEGER,
  batch_id        TEXT,
  rejected_reason TEXT,
  PRIMARY KEY (device_id, seq)
);
CREATE INDEX IF NOT EXISTS readings_pending
  ON readings (device_id, session_id, seq)
  WHERE uploaded_at IS NULL AND rejected_reason IS NULL;
CREATE TABLE IF NOT EXISTS sessions (
  session_id                TEXT PRIMARY KEY,
  device_id                 TEXT    NOT NULL,
  started_at                INTEGER NOT NULL,
  ended_at                  INTEGER,
  phone_time_at_connect     INTEGER NOT NULL,
  sensor_uptime_at_connect  INTEGER NOT NULL,
  sensor_boot_id_at_connect INTEGER NOT NULL,
  records_pulled            INTEGER NOT NULL DEFAULT 0,
  gaps                      INTEGER NOT NULL DEFAULT 0,
  error                     TEXT
);
`;

type DeviceRecord = {
  device_id: string;
  name: string | null;
  last_seen_at: number | null;
  last_boot_id: number | null;
  sensor_next_seq: number | null;
  sensor_collected_through: number;
  sensor_secured_through: number;
  server_acked_through: number;
  pending_on_sensor: number | null;
  fw_version: string | null;
};

type ReadingRecord = {
  device_id: string;
  seq: number;
  type: number;
  quality: number;
  flags: number;
  raw_time: number;
  boot_id: number;
  value: number;
  raw: string;
  recorded_at: number | null;
  time_source: string;
  session_id: string;
  collected_at: number;
  uploaded_at: number | null;
  batch_id: string | null;
  rejected_reason: string | null;
};

type SessionRecord = {
  session_id: string;
  device_id: string;
  started_at: number;
  ended_at: number | null;
  phone_time_at_connect: number;
  sensor_uptime_at_connect: number;
  sensor_boot_id_at_connect: number;
  records_pulled: number;
  gaps: number;
  error: string | null;
};

const DEVICE_COLUMNS: Record<keyof DeviceRow, string> = {
  deviceId: 'device_id',
  name: 'name',
  lastSeenAt: 'last_seen_at',
  lastBootId: 'last_boot_id',
  sensorNextSeq: 'sensor_next_seq',
  sensorCollectedThrough: 'sensor_collected_through',
  sensorSecuredThrough: 'sensor_secured_through',
  serverAckedThrough: 'server_acked_through',
  pendingOnSensor: 'pending_on_sensor',
  fwVersion: 'fw_version',
};

function toDevice(r: DeviceRecord): DeviceRow {
  return {
    deviceId: r.device_id,
    name: r.name,
    lastSeenAt: r.last_seen_at,
    lastBootId: r.last_boot_id,
    sensorNextSeq: r.sensor_next_seq,
    sensorCollectedThrough: r.sensor_collected_through,
    sensorSecuredThrough: r.sensor_secured_through,
    serverAckedThrough: r.server_acked_through,
    pendingOnSensor: r.pending_on_sensor,
    fwVersion: r.fw_version,
  };
}

function toReading(r: ReadingRecord): StoredReading {
  return {
    deviceId: r.device_id,
    seq: r.seq,
    type: r.type,
    quality: r.quality,
    flags: r.flags,
    rawTime: r.raw_time,
    bootId: r.boot_id,
    value: r.value,
    raw: r.raw,
    recordedAt: r.recorded_at,
    timeSource: r.time_source as TimeSource,
    sessionId: r.session_id,
    collectedAt: r.collected_at,
    uploadedAt: r.uploaded_at,
    batchId: r.batch_id,
    rejectedReason: r.rejected_reason,
  };
}

function toSession(r: SessionRecord): SessionRow {
  return {
    sessionId: r.session_id,
    deviceId: r.device_id,
    startedAt: r.started_at,
    endedAt: r.ended_at,
    phoneTimeAtConnect: r.phone_time_at_connect,
    sensorUptimeAtConnect: r.sensor_uptime_at_connect,
    sensorBootIdAtConnect: r.sensor_boot_id_at_connect,
    recordsPulled: r.records_pulled,
    gaps: r.gaps,
    error: r.error,
  };
}

function placeholders(n: number): string {
  return Array.from({ length: n }, () => '?').join(',');
}

export class SqliteRepository implements Repository {
  private db: SQLite.SQLiteDatabase | null = null;

  constructor(private readonly filename = 'telemetry.db') {}

  private get conn(): SQLite.SQLiteDatabase {
    if (!this.db) throw new Error('repository not initialised; call init()');
    return this.db;
  }

  async init(): Promise<void> {
    if (this.db) return;
    const db = await SQLite.openDatabaseAsync(this.filename);
    await db.execAsync('PRAGMA journal_mode = WAL; PRAGMA foreign_keys = ON;');
    await this.migrate(db);
    this.db = db;
  }

  private async migrate(db: SQLite.SQLiteDatabase): Promise<void> {
    const row = await db.getFirstAsync<{ user_version: number }>('PRAGMA user_version');
    const current = row?.user_version ?? 0;
    if (current >= SCHEMA_VERSION) return;
    await db.withTransactionAsync(async () => {
      if (current < 1) await db.execAsync(SCHEMA);
      await db.execAsync(`PRAGMA user_version = ${SCHEMA_VERSION}`);
    });
  }

  // --- settings ---

  async getSetting(key: string): Promise<string | null> {
    const row = await this.conn.getFirstAsync<{ value: string }>('SELECT value FROM settings WHERE key = ?', [key]);
    return row?.value ?? null;
  }

  async setSetting(key: string, value: string): Promise<void> {
    await this.conn.runAsync(
      'INSERT INTO settings (key, value) VALUES (?, ?) ON CONFLICT(key) DO UPDATE SET value = excluded.value',
      [key, value],
    );
  }

  // --- devices ---

  async upsertDevice(row: Partial<DeviceRow> & { deviceId: string }): Promise<void> {
    const keys = (Object.keys(row) as (keyof DeviceRow)[]).filter((k) => row[k] !== undefined);
    const cols = keys.map((k) => DEVICE_COLUMNS[k]);
    const values = keys.map((k) => row[k] as SQLite.SQLiteBindValue);
    const updates = cols.filter((c) => c !== 'device_id').map((c) => `${c} = excluded.${c}`);
    const sql =
      `INSERT INTO devices (${cols.join(',')}) VALUES (${placeholders(cols.length)}) ` +
      (updates.length ? `ON CONFLICT(device_id) DO UPDATE SET ${updates.join(', ')}` : 'ON CONFLICT(device_id) DO NOTHING');
    await this.conn.runAsync(sql, values);
  }

  async getDevice(deviceId: string): Promise<DeviceRow | null> {
    const r = await this.conn.getFirstAsync<DeviceRecord>('SELECT * FROM devices WHERE device_id = ?', [deviceId]);
    return r ? toDevice(r) : null;
  }

  async listDevices(): Promise<DeviceRow[]> {
    const rows = await this.conn.getAllAsync<DeviceRecord>('SELECT * FROM devices ORDER BY last_seen_at DESC NULLS LAST');
    return rows.map(toDevice);
  }

  // --- readings ---

  async insertReadings(rows: StoredReading[]): Promise<number> {
    if (rows.length === 0) return 0;
    let inserted = 0;
    await this.conn.withTransactionAsync(async () => {
      for (const r of rows) {
        const res = await this.conn.runAsync(
          `INSERT OR IGNORE INTO readings
             (device_id, seq, type, quality, flags, raw_time, boot_id, value, raw, recorded_at, time_source,
              session_id, collected_at, uploaded_at, batch_id, rejected_reason)
           VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)`,
          [
            r.deviceId, r.seq, r.type, r.quality, r.flags, r.rawTime, r.bootId, r.value, r.raw, r.recordedAt,
            r.timeSource, r.sessionId, r.collectedAt, r.uploadedAt, r.batchId, r.rejectedReason,
          ],
        );
        inserted += res.changes;
      }
    });
    return inserted;
  }

  async maxSeq(deviceId: string): Promise<number> {
    const r = await this.conn.getFirstAsync<{ m: number | null }>('SELECT MAX(seq) AS m FROM readings WHERE device_id = ?', [deviceId]);
    return r?.m ?? 0;
  }

  async countReadings(deviceId: string): Promise<ReadingCounts> {
    const r = await this.conn.getFirstAsync<{ collected: number; uploaded: number }>(
      `SELECT COUNT(*) AS collected, COUNT(uploaded_at) AS uploaded FROM readings WHERE device_id = ?`,
      [deviceId],
    );
    return { collected: r?.collected ?? 0, uploaded: r?.uploaded ?? 0 };
  }

  async listReadings(deviceId: string, limit: number): Promise<StoredReading[]> {
    const rows = await this.conn.getAllAsync<ReadingRecord>(
      'SELECT * FROM readings WHERE device_id = ? ORDER BY seq DESC LIMIT ?',
      [deviceId, limit],
    );
    return rows.map(toReading);
  }

  async listUnuploaded(deviceId: string, sessionId: string, limit: number): Promise<StoredReading[]> {
    const rows = await this.conn.getAllAsync<ReadingRecord>(
      `SELECT * FROM readings
        WHERE device_id = ? AND session_id = ? AND uploaded_at IS NULL AND rejected_reason IS NULL
        ORDER BY seq ASC LIMIT ?`,
      [deviceId, sessionId, limit],
    );
    return rows.map(toReading);
  }

  async sessionsWithUnuploaded(deviceId: string): Promise<string[]> {
    const rows = await this.conn.getAllAsync<{ session_id: string }>(
      `SELECT session_id, MIN(seq) AS first_seq FROM readings
        WHERE device_id = ? AND uploaded_at IS NULL AND rejected_reason IS NULL
        GROUP BY session_id ORDER BY first_seq ASC`,
      [deviceId],
    );
    return rows.map((r) => r.session_id);
  }

  async devicesWithUnuploaded(): Promise<string[]> {
    const rows = await this.conn.getAllAsync<{ device_id: string }>(
      `SELECT DISTINCT device_id FROM readings WHERE uploaded_at IS NULL AND rejected_reason IS NULL ORDER BY device_id`,
    );
    return rows.map((r) => r.device_id);
  }

  async markUploaded(deviceId: string, seqs: number[], batchId: string, uploadedAt: number): Promise<void> {
    if (seqs.length === 0) return;
    await this.conn.runAsync(
      `UPDATE readings SET uploaded_at = ?, batch_id = ? WHERE device_id = ? AND seq IN (${placeholders(seqs.length)})`,
      [uploadedAt, batchId, deviceId, ...seqs],
    );
  }

  async markRejected(deviceId: string, seqs: number[], reason: string): Promise<void> {
    if (seqs.length === 0) return;
    await this.conn.runAsync(
      `UPDATE readings SET rejected_reason = ? WHERE device_id = ? AND seq IN (${placeholders(seqs.length)})`,
      [reason, deviceId, ...seqs],
    );
  }

  async uploadedSeqsAbove(deviceId: string, from: number): Promise<number[]> {
    const rows = await this.conn.getAllAsync<{ seq: number }>(
      'SELECT seq FROM readings WHERE device_id = ? AND uploaded_at IS NOT NULL AND seq > ? ORDER BY seq ASC',
      [deviceId, from],
    );
    return rows.map((r) => r.seq);
  }

  // --- sessions ---

  async insertSession(s: SessionRow): Promise<void> {
    await this.conn.runAsync(
      `INSERT INTO sessions (session_id, device_id, started_at, ended_at, phone_time_at_connect,
         sensor_uptime_at_connect, sensor_boot_id_at_connect, records_pulled, gaps, error)
       VALUES (?,?,?,?,?,?,?,?,?,?)`,
      [s.sessionId, s.deviceId, s.startedAt, s.endedAt, s.phoneTimeAtConnect, s.sensorUptimeAtConnect,
        s.sensorBootIdAtConnect, s.recordsPulled, s.gaps, s.error],
    );
  }

  async updateSession(s: SessionRow): Promise<void> {
    await this.conn.runAsync(
      `UPDATE sessions SET ended_at = ?, records_pulled = ?, gaps = ?, error = ? WHERE session_id = ?`,
      [s.endedAt, s.recordsPulled, s.gaps, s.error, s.sessionId],
    );
  }

  async getSession(sessionId: string): Promise<SessionRow | null> {
    const r = await this.conn.getFirstAsync<SessionRecord>('SELECT * FROM sessions WHERE session_id = ?', [sessionId]);
    return r ? toSession(r) : null;
  }

  async listSessions(limit: number): Promise<SessionRow[]> {
    const rows = await this.conn.getAllAsync<SessionRecord>('SELECT * FROM sessions ORDER BY started_at DESC LIMIT ?', [limit]);
    return rows.map(toSession);
  }
}
