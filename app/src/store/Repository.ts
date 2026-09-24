// The phone's durable state. SqliteRepository implements it on expo-sqlite;
// MemoryRepository is the twin for tests. Times are unix seconds.

export interface DeviceRow {
  deviceId: string;
  name: string | null;
  lastSeenAt: number | null;
  lastBootId: number | null;
  /** Sensor-reported cursors from the last visit. */
  sensorNextSeq: number | null;
  sensorCollectedThrough: number;
  sensorSecuredThrough: number;
  /** Highest seq contiguous from sensorSecuredThrough that the server holds. */
  serverAckedThrough: number;
  pendingOnSensor: number | null;
  fwVersion: string | null;
}

export type TimeSource = 'sensor' | 'phone_backfill' | 'unknown';

export interface StoredReading {
  deviceId: string;
  seq: number;
  type: number;
  quality: number;
  flags: number;
  rawTime: number;
  bootId: number;
  value: number;
  /** The 20 record bytes as hex, uploaded verbatim. */
  raw: string;
  recordedAt: number | null;
  timeSource: TimeSource;
  sessionId: string;
  collectedAt: number;
  uploadedAt: number | null;
  batchId: string | null;
  rejectedReason: string | null;
}

export interface SessionRow {
  sessionId: string;
  deviceId: string;
  startedAt: number;
  endedAt: number | null;
  phoneTimeAtConnect: number;
  sensorUptimeAtConnect: number;
  sensorBootIdAtConnect: number;
  recordsPulled: number;
  gaps: number;
  error: string | null;
}

export interface ReadingCounts {
  collected: number;
  uploaded: number;
}

export interface Repository {
  init(): Promise<void>;

  getSetting(key: string): Promise<string | null>;
  setSetting(key: string, value: string): Promise<void>;

  upsertDevice(row: Partial<DeviceRow> & { deviceId: string }): Promise<void>;
  getDevice(deviceId: string): Promise<DeviceRow | null>;
  listDevices(): Promise<DeviceRow[]>;

  /** Inserts, ignoring (deviceId, seq) pairs already held. Returns how many were new. */
  insertReadings(rows: StoredReading[]): Promise<number>;
  maxSeq(deviceId: string): Promise<number>;
  countReadings(deviceId: string): Promise<ReadingCounts>;
  listReadings(deviceId: string, limit: number): Promise<StoredReading[]>;

  /** Not yet uploaded and not rejected, oldest first, for one session. */
  listUnuploaded(deviceId: string, sessionId: string, limit: number): Promise<StoredReading[]>;
  /** Session ids that still have unuploaded readings for the device, oldest first. */
  sessionsWithUnuploaded(deviceId: string): Promise<string[]>;
  devicesWithUnuploaded(): Promise<string[]>;
  markUploaded(deviceId: string, seqs: number[], batchId: string, uploadedAt: number): Promise<void>;
  markRejected(deviceId: string, seqs: number[], reason: string): Promise<void>;
  /** Uploaded seqs strictly above `from`, ascending. */
  uploadedSeqsAbove(deviceId: string, from: number): Promise<number[]>;

  insertSession(row: SessionRow): Promise<void>;
  updateSession(row: SessionRow): Promise<void>;
  getSession(sessionId: string): Promise<SessionRow | null>;
  listSessions(limit: number): Promise<SessionRow[]>;
}

/** Highest seq such that every seq in (from, result] is in `sorted`. */
export function contiguousFrom(from: number, sortedAscending: number[]): number {
  let through = from;
  for (const s of sortedAscending) {
    if (s <= through) continue;
    if (s === through + 1) through = s;
    else break;
  }
  return through;
}
