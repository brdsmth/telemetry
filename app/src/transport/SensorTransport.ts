// What the collection service needs from a radio. BlePlxTransport implements
// it on react-native-ble-plx; FakeTransport implements it over FakeSensor for
// tests and for running the UI with no hardware.
import { Advertising, Command, DeviceInfo, Status } from '../protocol/ble';

export interface SensorAdvertisement {
  /** Transport-specific peripheral id (a UUID on iOS, a MAC on Android). */
  id: string;
  name: string | null;
  rssi: number | null;
  /** Decoded manufacturer data, null when absent or not ours. */
  advertising: Advertising | null;
}

export interface SensorLink {
  readonly id: string;
  /** Negotiated ATT MTU; payload per notification is mtu - 3. */
  readonly mtu: number;
  readDeviceInfo(): Promise<DeviceInfo>;
  /** Writes one command and resolves with the Status the sensor answers. */
  command(cmd: Command, timeoutMs?: number): Promise<Status>;
  /** Raw Data notifications. Returns an unsubscribe function. */
  onChunk(handler: (bytes: Uint8Array) => void): () => void;
  onDisconnect(handler: (reason: string) => void): () => void;
  disconnect(): Promise<void>;
}

export interface SensorTransport {
  /** Scans for the telemetry service for `durationMs`, reporting each sensor once. */
  scan(durationMs: number, onFound: (a: SensorAdvertisement) => void): Promise<void>;
  connect(id: string, timeoutMs?: number): Promise<SensorLink>;
}

export class TransportError extends Error {
  constructor(message: string) {
    super(message);
    this.name = 'TransportError';
  }
}
