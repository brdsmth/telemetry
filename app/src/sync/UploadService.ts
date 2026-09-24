// Pushes collected readings to the server and turns its acks into the
// secured watermark the next sensor visit will pass on. Idempotent: a retried
// batch gets the same answer from the server.
import { ApiClient, ApiError } from '../net/ApiClient';
import { fromHex, toBase64 } from '../protocol/bytes';
import { PROTOCOL_VERSION } from '../protocol/record';
import { contiguousFrom, Repository } from '../store/Repository';
import { SyncDeps } from './deps';

export interface UploadOptions {
  /** Records per request. The API accepts up to 5000. */
  batchSize?: number;
}

export interface DeviceUploadResult {
  deviceId: string;
  batches: number;
  uploaded: number;
  rejected: number;
  serverAckedThrough: number;
  error: string | null;
}

export interface UploadResult {
  devices: DeviceUploadResult[];
  uploaded: number;
  rejected: number;
  failed: number;
}

export const PHONE_ID_SETTING = 'phone_id';

export class UploadService {
  constructor(
    private readonly api: ApiClient,
    private readonly repo: Repository,
    private readonly deps: SyncDeps,
  ) {}

  /** The install's stable id, minted on first use. */
  async phoneId(): Promise<string> {
    let id = await this.repo.getSetting(PHONE_ID_SETTING);
    if (!id) {
      id = this.deps.uuid();
      await this.repo.setSetting(PHONE_ID_SETTING, id);
    }
    return id;
  }

  async uploadAll(opts: UploadOptions = {}): Promise<UploadResult> {
    const result: UploadResult = { devices: [], uploaded: 0, rejected: 0, failed: 0 };
    for (const deviceId of await this.repo.devicesWithUnuploaded()) {
      const r = await this.uploadDevice(deviceId, opts);
      result.devices.push(r);
      result.uploaded += r.uploaded;
      result.rejected += r.rejected;
      if (r.error) result.failed++;
    }
    return result;
  }

  async uploadDevice(deviceId: string, opts: UploadOptions = {}): Promise<DeviceUploadResult> {
    const batchSize = Math.min(opts.batchSize ?? 500, 5000);
    const phoneId = await this.phoneId();
    const out: DeviceUploadResult = { deviceId, batches: 0, uploaded: 0, rejected: 0, serverAckedThrough: 0, error: null };

    try {
      // One session per batch so the server gets the right connect context
      // for back-filling times.
      for (const sessionId of await this.repo.sessionsWithUnuploaded(deviceId)) {
        const session = await this.repo.getSession(sessionId);
        if (!session) {
          this.deps.log(`readings reference unknown session ${sessionId}; skipping`);
          continue;
        }
        for (;;) {
          const rows = await this.repo.listUnuploaded(deviceId, sessionId, batchSize);
          if (rows.length === 0) break;
          const batchId = this.deps.uuid();
          const response = await this.api.postBatch({
            batch_id: batchId,
            session_id: sessionId,
            phone_id: phoneId,
            device_id: deviceId,
            protocol_version: PROTOCOL_VERSION,
            phone_time_at_connect: session.phoneTimeAtConnect,
            sensor_uptime_at_connect: session.sensorUptimeAtConnect,
            sensor_boot_id_at_connect: session.sensorBootIdAtConnect,
            records: rows.map((r) => toBase64(fromHex(r.raw))),
          });
          out.batches++;

          const rejectedIdx = new Set(response.rejected.map((x) => x.index));
          const rejectedBy = new Map<string, number[]>();
          response.rejected.forEach((x) => {
            const list = rejectedBy.get(x.reason) ?? [];
            list.push(rows[x.index].seq);
            rejectedBy.set(x.reason, list);
          });
          for (const [reason, seqs] of rejectedBy) await this.repo.markRejected(deviceId, seqs, reason);
          out.rejected += rejectedIdx.size;

          const acked = rows
            .filter((r, i) => !rejectedIdx.has(i) && response.acked.some((a) => r.seq >= a.from_seq && r.seq <= a.to_seq))
            .map((r) => r.seq);
          await this.repo.markUploaded(deviceId, acked, batchId, this.deps.now());
          out.uploaded += acked.length;

          // Anything neither acked nor rejected stays pending; the server
          // will see it again. Stop if the server acked nothing, to avoid
          // spinning on the same rows.
          if (acked.length === 0 && rejectedIdx.size === 0) {
            throw new ApiError(200, 'server acked nothing for a non-empty batch', false);
          }
          if (rows.length < batchSize) break;
        }
      }
    } catch (e) {
      out.error = e instanceof Error ? e.message : String(e);
      this.deps.log(`upload ${deviceId}: ${out.error}`);
    }

    // Recompute the secured watermark from what has been uploaded, starting
    // at what the sensor already knows is secured.
    const device = await this.repo.getDevice(deviceId);
    const base = device?.sensorSecuredThrough ?? 0;
    const uploadedAbove = await this.repo.uploadedSeqsAbove(deviceId, base);
    out.serverAckedThrough = Math.max(device?.serverAckedThrough ?? 0, contiguousFrom(base, uploadedAbove));
    await this.repo.upsertDevice({ deviceId, serverAckedThrough: out.serverAckedThrough });
    return out;
  }
}
