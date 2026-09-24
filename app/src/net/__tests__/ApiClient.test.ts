import { ApiError, FetchApiClient } from '../ApiClient';

const envelope = {
  batch_id: 'b', session_id: 's', phone_id: 'p', device_id: 'aabbccddeeff', protocol_version: 1,
  phone_time_at_connect: 1, sensor_uptime_at_connect: 1, sensor_boot_id_at_connect: 1, records: ['AA=='],
};

let calls: { url: string; init?: RequestInit }[] = [];
beforeEach(() => {
  calls = [];
});

function stubFetch(status: number, body: unknown): typeof fetch {
  return (async (input: RequestInfo | URL, init?: RequestInit) => {
    calls.push({ url: String(input), init });
    return new Response(typeof body === 'string' ? body : JSON.stringify(body), {
      status,
      headers: { 'Content-Type': 'application/json' },
    });
  }) as typeof fetch;
}

const failing = (async () => {
  throw new TypeError('offline');
}) as unknown as typeof fetch;

test('posts the envelope as json to /v1/batches and returns the response', async () => {
  const client = new FetchApiClient('https://api.example', stubFetch(200, { batch_id: 'b', acked: [{ from_seq: 1, to_seq: 1 }], rejected: [] }));
  const res = await client.postBatch(envelope);
  expect(res.acked).toEqual([{ from_seq: 1, to_seq: 1 }]);
  expect(calls[0].url).toBe('https://api.example/v1/batches');
  expect(calls[0].init?.method).toBe('POST');
  expect(JSON.parse(String(calls[0].init?.body)).device_id).toBe('aabbccddeeff');
});

test('4xx is not retryable, 5xx and network failures are', async () => {
  await expect(new FetchApiClient('x', stubFetch(400, 'bad')).postBatch(envelope)).rejects.toMatchObject({ status: 400, retryable: false });
  await expect(new FetchApiClient('x', stubFetch(503, 'down')).postBatch(envelope)).rejects.toMatchObject({ status: 503, retryable: true });
  const err = await new FetchApiClient('x', failing).postBatch(envelope).catch((e) => e);
  expect(err).toBeInstanceOf(ApiError);
  expect(err.retryable).toBe(true);
});

test('health reflects the status and swallows network errors', async () => {
  expect(await new FetchApiClient('x', stubFetch(200, 'ok')).health()).toBe(true);
  expect(await new FetchApiClient('x', stubFetch(503, 'no')).health()).toBe(false);
  expect(await new FetchApiClient('x', failing).health()).toBe(false);
});

test('the default fetch is not invoked as a method of the client', async () => {
  const original = globalThis.fetch;
  globalThis.fetch = async function (this: unknown) {
    // Browsers throw exactly this when fetch's `this` is not the window.
    if (this instanceof FetchApiClient) throw new TypeError("Failed to execute 'fetch' on 'Window': Illegal invocation");
    return new Response('ok', { status: 200 });
  } as typeof fetch;
  try {
    expect(await new FetchApiClient('x').health()).toBe(true);
  } finally {
    globalThis.fetch = original;
  }
});
