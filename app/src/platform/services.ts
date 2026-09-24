// Wires the real implementations together once per app run. Screens call
// getServices() and never construct transports or repositories themselves.
import * as Crypto from 'expo-crypto';
import { Platform } from 'react-native';

import { ApiClient, FetchApiClient } from '../net/ApiClient';
import { Repository } from '../store/Repository';
import { SqliteRepository } from '../store/SqliteRepository';
import { CollectionService } from '../sync/CollectionService';
import { SyncDeps } from '../sync/deps';
import { UploadService } from '../sync/UploadService';
import { FakeSensor } from '../transport/FakeSensor';
import { FakeTransport } from '../transport/FakeTransport';
import { SensorTransport } from '../transport/SensorTransport';

export const DEFAULT_API_URL = 'https://api-production-2e52.up.railway.app';
export const API_URL_SETTING = 'api_url';
export const FAKE_SENSORS_SETTING = 'use_fake_sensors';

export interface Services {
  repo: Repository;
  transport: SensorTransport;
  api: ApiClient;
  collection: CollectionService;
  upload: UploadService;
  deps: SyncDeps;
  apiUrl: string;
  usingFakeSensors: boolean;
  /** Recent log lines from the services, newest last. */
  log: string[];
}

let instance: Promise<Services> | null = null;

export function getServices(): Promise<Services> {
  if (!instance) instance = build();
  return instance;
}

/** Drop the singleton so the next getServices() re-reads settings. */
export function resetServices(): void {
  instance = null;
}

async function build(): Promise<Services> {
  const repo = new SqliteRepository();
  await repo.init();

  const log: string[] = [];
  const deps: SyncDeps = {
    now: () => Math.floor(Date.now() / 1000),
    uuid: () => Crypto.randomUUID(),
    log: (m) => {
      log.push(`${new Date().toISOString().slice(11, 19)} ${m}`);
      if (log.length > 200) log.shift();
    },
  };

  const apiUrl = (await repo.getSetting(API_URL_SETTING)) ?? DEFAULT_API_URL;
  // The web target has no BLE; fall back to fake sensors there, or when the
  // setting asks for them (simulator development).
  const usingFakeSensors = Platform.OS === 'web' || (await repo.getSetting(FAKE_SENSORS_SETTING)) === 'true';
  const transport = usingFakeSensors ? demoTransport() : await realTransport();
  const api = new FetchApiClient(apiUrl);

  return {
    repo,
    transport,
    api,
    collection: new CollectionService(transport, repo, deps),
    upload: new UploadService(api, repo, deps),
    deps,
    apiUrl,
    usingFakeSensors,
    log,
  };
}

async function realTransport(): Promise<SensorTransport> {
  // Loaded lazily so the web bundle never touches the native module.
  const { BlePlxTransport } = await import('../transport/BlePlxTransport');
  return new BlePlxTransport();
}

function demoTransport(): SensorTransport {
  const a = new FakeSensor({ deviceId: 'fa4e000000a1', bootId: 2, fwVersion: 'fake-0.1.0', batteryMv: 3650 });
  const b = new FakeSensor({ deviceId: 'fa4e000000b2', bootId: 5, fwVersion: 'fake-0.1.0', batteryMv: 3410 });
  for (let i = 0; i < 90; i++) {
    a.advance(60);
    a.sample(80000 + Math.round(Math.sin(i / 9) * 4000));
    if (i % 3 === 0) {
      b.advance(180);
      b.sample(120000 - i * 300);
    }
  }
  // Keep the fakes sampling while the app runs.
  setInterval(() => {
    a.advance(60);
    a.sample(80000 + Math.round(Math.random() * 500));
  }, 60_000);
  return new FakeTransport([a, b]);
}
