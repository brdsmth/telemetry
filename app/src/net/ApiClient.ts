// POST /v1/batches, schema/PROTOCOL.md §4. FetchApiClient talks to the real
// API; FakeApiClient records envelopes for tests.

export interface BatchEnvelope {
  batch_id: string;
  session_id: string;
  phone_id: string;
  device_id: string;
  protocol_version: number;
  phone_time_at_connect: number;
  sensor_uptime_at_connect: number;
  sensor_boot_id_at_connect: number;
  /** Base64 of the 20 record bytes. */
  records: string[];
}

export interface AckedRange {
  from_seq: number;
  to_seq: number;
}

export interface BatchResponse {
  batch_id: string;
  acked: AckedRange[];
  rejected: { index: number; reason: string }[];
}

export class ApiError extends Error {
  constructor(
    public readonly status: number,
    message: string,
    /** True for 5xx and network failures: the same batch may be retried. */
    public readonly retryable: boolean,
  ) {
    super(message);
    this.name = 'ApiError';
  }
}

export interface ApiClient {
  postBatch(envelope: BatchEnvelope): Promise<BatchResponse>;
  health(): Promise<boolean>;
}

export class FetchApiClient implements ApiClient {
  // Wrapped in an arrow so `fetch` runs with its own `this`; browsers throw
  // "Illegal invocation" when it is called as a method of another object.
  constructor(
    private readonly baseUrl: string,
    private readonly fetchImpl: typeof fetch = (input, init) => fetch(input, init),
  ) {}

  async postBatch(envelope: BatchEnvelope): Promise<BatchResponse> {
    let res: Response;
    try {
      res = await this.fetchImpl(`${this.baseUrl}/v1/batches`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(envelope),
      });
    } catch (e) {
      throw new ApiError(0, `network: ${String(e)}`, true);
    }
    if (!res.ok) {
      const text = await res.text().catch(() => '');
      throw new ApiError(res.status, `HTTP ${res.status}: ${text.trim()}`, res.status >= 500);
    }
    return (await res.json()) as BatchResponse;
  }

  async health(): Promise<boolean> {
    try {
      const res = await this.fetchImpl(`${this.baseUrl}/healthz`);
      return res.ok;
    } catch {
      return false;
    }
  }
}

export class FakeApiClient implements ApiClient {
  envelopes: BatchEnvelope[] = [];
  /** Test hook: seqs the server should claim it already rejects (by index). */
  rejectIndices = new Set<number>();
  /** Test hook: fail the next N posts with this error. */
  failNext = 0;
  failWith: ApiError = new ApiError(503, 'service unavailable', true);
  /** Test hook: pretend the server already held these seqs per device. */
  alreadyHeld = new Map<string, Set<number>>();
  healthy = true;

  constructor(private readonly decodeSeq: (base64Record: string) => number) {}

  async postBatch(envelope: BatchEnvelope): Promise<BatchResponse> {
    if (this.failNext > 0) {
      this.failNext--;
      throw this.failWith;
    }
    this.envelopes.push(envelope);
    const held = this.alreadyHeld.get(envelope.device_id) ?? new Set<number>();
    this.alreadyHeld.set(envelope.device_id, held);
    const rejected: BatchResponse['rejected'] = [];
    let min = Infinity;
    let max = -Infinity;
    envelope.records.forEach((r, i) => {
      const seq = this.decodeSeq(r);
      if (this.rejectIndices.has(i)) {
        rejected.push({ index: i, reason: 'bad_crc' });
        return;
      }
      held.add(seq);
      min = Math.min(min, seq);
      max = Math.max(max, seq);
    });
    const acked: AckedRange[] = [];
    if (min !== Infinity) {
      const seqs = [...held].filter((s) => s >= min && s <= max).sort((a, b) => a - b);
      for (const s of seqs) {
        const last = acked[acked.length - 1];
        if (last && last.to_seq + 1 === s) last.to_seq = s;
        else acked.push({ from_seq: s, to_seq: s });
      }
    }
    return { batch_id: envelope.batch_id, acked, rejected };
  }

  async health(): Promise<boolean> {
    return this.healthy;
  }
}
