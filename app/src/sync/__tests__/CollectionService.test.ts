import { Opcode } from '../../protocol/ble';
import { MemoryRepository } from '../../store/MemoryRepository';
import { FakeSensor } from '../../transport/FakeSensor';
import { FakeTransport } from '../../transport/FakeTransport';
import { CollectionError, CollectionService } from '../CollectionService';
import { testDeps } from '../deps';

const DEVICE = 'aabbccddeeff';

function rig(records: number, sensorOpts: Partial<ConstructorParameters<typeof FakeSensor>[0]> = {}) {
  const sensor = new FakeSensor({ deviceId: DEVICE, bootId: 3, ...sensorOpts });
  for (let i = 0; i < records; i++) {
    sensor.advance(60);
    sensor.sample(1000 + i);
  }
  const transport = new FakeTransport([sensor]);
  const repo = new MemoryRepository();
  const deps = testDeps();
  const service = new CollectionService(transport, repo, deps);
  return { sensor, transport, repo, deps, service };
}

test('first visit pulls everything, stores it, acks collected and sets the clock', async () => {
  const { sensor, repo, service, deps } = rig(30);
  const uptimeAtConnect = sensor.uptimeS;

  const result = await service.collect(DEVICE);

  expect(result).toMatchObject({ deviceId: DEVICE, fromSeq: 1, throughSeq: 30, pulled: 30, newlyStored: 30, gaps: 0, pendingAfter: 0 });
  expect(sensor.cursor()).toMatchObject({ collectedThrough: 30, securedThrough: 0 });
  expect(sensor.size()).toBe(30); // nothing freed until the server confirms
  expect(sensor.unixTime()).toBe(deps.clock.value);
  expect(await repo.countReadings(DEVICE)).toEqual({ collected: 30, uploaded: 0 });

  // Records were uptime-stamped; the phone back-filled them from connect time.
  const rows = await repo.listReadings(DEVICE, 1);
  expect(rows[0].timeSource).toBe('phone_backfill');
  expect(rows[0].recordedAt).toBe(deps.clock.value - uptimeAtConnect + rows[0].rawTime);
  expect(rows[0].raw).toHaveLength(40);

  const device = (await repo.getDevice(DEVICE))!;
  expect(device).toMatchObject({ lastBootId: 3, sensorCollectedThrough: 30, pendingOnSensor: 0 });
  const sessions = await repo.listSessions(10);
  expect(sessions).toHaveLength(1);
  expect(sessions[0]).toMatchObject({ deviceId: DEVICE, recordsPulled: 30, gaps: 0, error: null, sensorBootIdAtConnect: 3 });
  expect(sessions[0].endedAt).not.toBeNull();
  expect(sensor.commandLog.map((c) => c.opcode)).toEqual([
    Opcode.OpenSession, Opcode.SetTime, Opcode.ReadFrom, Opcode.AckCollected, Opcode.CloseSession,
  ]);
});

test('second visit pulls only what is new and stays idempotent', async () => {
  const { sensor, repo, service } = rig(10);
  await service.collect(DEVICE);
  for (let i = 0; i < 4; i++) {
    sensor.advance(60);
    sensor.sample(2000 + i);
  }
  const r = await service.collect(DEVICE);
  expect(r).toMatchObject({ fromSeq: 11, throughSeq: 14, pulled: 4, newlyStored: 4 });
  expect(await repo.countReadings(DEVICE)).toEqual({ collected: 14, uploaded: 0 });

  const again = await service.collect(DEVICE);
  expect(again).toMatchObject({ pulled: 0, newlyStored: 0, throughSeq: 0 });
  expect(sensor.cursor().collectedThrough).toBe(14);
});

test('windows larger than one chunk and multiple windows are pulled in order', async () => {
  const { repo, service } = rig(100);
  const r = await service.collect(DEVICE, { windowSize: 40 });
  expect(r).toMatchObject({ pulled: 100, throughSeq: 100, gaps: 0 });
  const rows = await repo.listReadings(DEVICE, 100);
  expect(rows.map((x) => x.seq)).toEqual(Array.from({ length: 100 }, (_, i) => 100 - i));
});

test('a wiped phone re-pulls from what it holds even though the sensor thinks it was collected', async () => {
  const { transport, service: first } = rig(20);
  await first.collect(DEVICE);
  const freshRepo = new MemoryRepository();
  const second = new CollectionService(transport, freshRepo, testDeps());
  const r = await second.collect(DEVICE);
  expect(r).toMatchObject({ fromSeq: 1, pulled: 20, newlyStored: 20 });
});

test('secured ack from the server is passed on at the next visit and frees slots', async () => {
  const { sensor, repo, service } = rig(10);
  await service.collect(DEVICE);
  // The upload service would set this after the server acked 1..7.
  await repo.upsertDevice({ deviceId: DEVICE, serverAckedThrough: 7 });

  const r = await service.collect(DEVICE);
  expect(r.securedAckSent).toBe(7);
  expect(sensor.cursor()).toMatchObject({ securedThrough: 7, tailSeq: 8 });
  expect(sensor.size()).toBe(3);
  expect((await repo.getDevice(DEVICE))!.sensorSecuredThrough).toBe(7);
});

test('gaps from corrupt slots are counted and the rest is stored', async () => {
  const { sensor, repo, service } = rig(10);
  sensor.corruptSeqs.add(4);
  const r = await service.collect(DEVICE);
  expect(r).toMatchObject({ pulled: 9, gaps: 1, throughSeq: 10 });
  const seqs = (await repo.listReadings(DEVICE, 20)).map((x) => x.seq).sort((a, b) => a - b);
  expect(seqs).toEqual([1, 2, 3, 5, 6, 7, 8, 9, 10]);
});

test('records with epoch time keep the sensor time source', async () => {
  const { sensor, repo, service } = rig(0);
  sensor.handleCommand(new Uint8Array([Opcode.OpenSession, ...new Array(16).fill(0)]), 512);
  sensor.handleCommand(new Uint8Array([Opcode.SetTime, 0x80, 0x16, 0xb3, 0x6a]), 512); // 1790121600
  sensor.onDisconnect();
  sensor.advance(60);
  sensor.sample(1);
  await service.collect(DEVICE);
  const [row] = await repo.listReadings(DEVICE, 1);
  expect(row.timeSource).toBe('sensor');
  expect(row.recordedAt).toBe(1_790_121_660);
});

test('records from an earlier boot without epoch stay unknown', async () => {
  const { sensor, repo, service } = rig(3);
  sensor.reboot(); // boot 4, records 1..3 belong to boot 3
  sensor.advance(30);
  sensor.sample(9);
  await service.collect(DEVICE);
  const rows = await repo.listReadings(DEVICE, 10);
  expect(rows.find((x) => x.seq === 1)!.timeSource).toBe('unknown');
  expect(rows.find((x) => x.seq === 4)!.timeSource).toBe('phone_backfill');
});

test('a dropped chunk surfaces as a timeout window, keeps what arrived, and records the session', async () => {
  const { transport, repo, service } = rig(30);
  const origConnect = transport.connect.bind(transport);
  transport.connect = async (id) => {
    const link = await origConnect(id);
    (link as any).dropChunkIndices.add(1); // lose the last chunk of the first window
    return link;
  };
  const r = await service.collect(DEVICE, { chunkTimeoutMs: 20 });
  expect(r.pulled).toBe(24);
  expect(r.throughSeq).toBe(24);
  expect(await repo.countReadings(DEVICE)).toEqual({ collected: 24, uploaded: 0 });
});

test('a link loss mid-visit records the error on the session and rethrows', async () => {
  const { transport, repo, service } = rig(5);
  const origConnect = transport.connect.bind(transport);
  transport.connect = async (id) => {
    const link = await origConnect(id);
    (link as any).disconnectAfterCommands = 1; // open ok, then gone
    return link;
  };
  await expect(service.collect(DEVICE)).rejects.toThrow();
  const [session] = await repo.listSessions(1);
  expect(session.error).toMatch(/disconnected|not connected/);
  expect(session.endedAt).not.toBeNull();
});

test('an unreachable sensor fails before any session is created', async () => {
  const { transport, repo, service } = rig(1);
  transport.unreachable.add(DEVICE);
  await expect(service.collect(DEVICE)).rejects.toThrow(/cannot reach/);
  expect(await repo.listSessions(1)).toHaveLength(0);
});

test('a sensor on another protocol version is refused', async () => {
  const { sensor, service } = rig(1);
  const real = sensor.deviceInfo.bind(sensor);
  sensor.deviceInfo = () => ({ ...real(), protocolVersion: 2 });
  await expect(service.collect(DEVICE)).rejects.toThrow(CollectionError);
});
