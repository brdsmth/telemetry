import { ApiError, FakeApiClient } from '../../net/ApiClient';
import { fromBase64 } from '../../protocol/bytes';
import { decodeRecord } from '../../protocol/record';
import { MemoryRepository } from '../../store/MemoryRepository';
import { FakeSensor } from '../../transport/FakeSensor';
import { FakeTransport } from '../../transport/FakeTransport';
import { CollectionService } from '../CollectionService';
import { testDeps } from '../deps';
import { PHONE_ID_SETTING, UploadService } from '../UploadService';

const DEVICE = 'aabbccddeeff';

async function collected(records: number) {
  const sensor = new FakeSensor({ deviceId: DEVICE, bootId: 3 });
  for (let i = 0; i < records; i++) {
    sensor.advance(60);
    sensor.sample(1000 + i);
  }
  const transport = new FakeTransport([sensor]);
  const repo = new MemoryRepository();
  const deps = testDeps();
  const collection = new CollectionService(transport, repo, deps);
  await collection.collect(DEVICE);
  const api = new FakeApiClient((b64) => decodeRecord(fromBase64(b64)).seq);
  const upload = new UploadService(api, repo, deps);
  return { sensor, transport, repo, deps, collection, api, upload };
}

test('uploads collected readings in session-scoped batches and records the server ack', async () => {
  const { repo, api, upload } = await collected(30);

  const r = await upload.uploadAll({ batchSize: 12 });

  expect(r).toMatchObject({ uploaded: 30, rejected: 0, failed: 0 });
  expect(r.devices[0]).toMatchObject({ deviceId: DEVICE, batches: 3, serverAckedThrough: 30, error: null });
  expect(api.envelopes).toHaveLength(3);
  const env = api.envelopes[0];
  expect(env).toMatchObject({ device_id: DEVICE, protocol_version: 1, sensor_boot_id_at_connect: 3 });
  expect(env.records).toHaveLength(12);
  expect(env.session_id).toBe((await repo.listSessions(1))[0].sessionId);
  expect(env.phone_id).toBe(await repo.getSetting(PHONE_ID_SETTING));
  expect(await repo.countReadings(DEVICE)).toEqual({ collected: 30, uploaded: 30 });
  expect((await repo.getDevice(DEVICE))!.serverAckedThrough).toBe(30);
});

test('a second upload has nothing to send', async () => {
  const { api, upload } = await collected(5);
  await upload.uploadAll();
  const r = await upload.uploadAll();
  expect(r.uploaded).toBe(0);
  expect(api.envelopes).toHaveLength(1);
});

test('a retryable failure leaves readings pending and is retried next time', async () => {
  const { repo, api, upload } = await collected(5);
  api.failNext = 1;
  const first = await upload.uploadAll();
  expect(first.failed).toBe(1);
  expect(first.devices[0].error).toMatch(/service unavailable/);
  expect(await repo.countReadings(DEVICE)).toEqual({ collected: 5, uploaded: 0 });

  const second = await upload.uploadAll();
  expect(second).toMatchObject({ uploaded: 5, failed: 0 });
});

test('rejected records are marked and skipped, and the secured watermark stops before the hole', async () => {
  const { repo, api, upload } = await collected(6);
  api.rejectIndices.add(2); // seq 3
  const r = await upload.uploadAll();
  expect(r).toMatchObject({ uploaded: 5, rejected: 1 });
  expect(r.devices[0].serverAckedThrough).toBe(2);
  const rows = await repo.listReadings(DEVICE, 10);
  expect(rows.find((x) => x.seq === 3)!.rejectedReason).toBe('bad_crc');
  expect(await repo.devicesWithUnuploaded()).toEqual([]);
});

test('the secured watermark starts from what the sensor already knows is secured', async () => {
  const { repo, upload } = await collected(10);
  // Pretend an earlier visit already secured through 4 on the sensor.
  await repo.upsertDevice({ deviceId: DEVICE, sensorSecuredThrough: 4 });
  const r = await upload.uploadAll();
  expect(r.devices[0].serverAckedThrough).toBe(10);
});

test('records the server already held are acked, not duplicated', async () => {
  const { api, upload } = await collected(4);
  api.alreadyHeld.set(DEVICE, new Set([1, 2]));
  const r = await upload.uploadAll();
  expect(r.uploaded).toBe(4);
  expect(r.devices[0].serverAckedThrough).toBe(4);
});

test('a server that acks nothing stops the loop instead of spinning', async () => {
  const { api, upload } = await collected(3);
  api.postBatch = async (env) => ({ batch_id: env.batch_id, acked: [], rejected: [] });
  const r = await upload.uploadAll();
  expect(r.failed).toBe(1);
  expect(r.devices[0].error).toMatch(/acked nothing/);
});

test('phone id is minted once and reused', async () => {
  const { repo, upload } = await collected(0);
  const a = await upload.phoneId();
  const b = await upload.phoneId();
  expect(a).toBe(b);
  expect(await repo.getSetting(PHONE_ID_SETTING)).toBe(a);
});

test('ApiError marks 5xx and network failures retryable and 4xx not', () => {
  expect(new ApiError(503, 'x', true).retryable).toBe(true);
  expect(new ApiError(400, 'x', false).retryable).toBe(false);
});

test('full loop: collect, upload, then the next visit frees the sensor', async () => {
  const { sensor, upload, collection } = await collected(12);
  await upload.uploadAll();
  for (let i = 0; i < 3; i++) {
    sensor.advance(60);
    sensor.sample(5);
  }
  const visit = await collection.collect(DEVICE);
  expect(visit.securedAckSent).toBe(12);
  expect(visit.pulled).toBe(3);
  expect(sensor.cursor()).toMatchObject({ securedThrough: 12, collectedThrough: 15, tailSeq: 13 });
  expect(sensor.size()).toBe(3);
});
