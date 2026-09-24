// SensorTransport over FakeSensor instances. Chunks are delivered
// asynchronously after the Status resolves, in the order a BLE stack would
// notify them. Test hooks let a test drop chunks or fail a connection.
import { Command, DeviceInfo, encodeCommand, decodeStatus, encodeStatus, Status } from '../protocol/ble';
import { FakeSensor } from './FakeSensor';
import { SensorAdvertisement, SensorLink, SensorTransport, TransportError } from './SensorTransport';

export interface FakeTransportOptions {
  mtu?: number;
  /** Peripheral id to use for a sensor; defaults to the device id. */
  peripheralId?: (s: FakeSensor) => string;
}

export class FakeLink implements SensorLink {
  readonly id: string;
  readonly mtu: number;
  private chunkHandlers = new Set<(b: Uint8Array) => void>();
  private disconnectHandlers = new Set<(reason: string) => void>();
  private connected = true;
  /** Test hook: indices of chunks (per READ_FROM) to drop, simulating loss. */
  dropChunkIndices = new Set<number>();
  /** Test hook: disconnect after this many commands. */
  disconnectAfterCommands: number | null = null;
  commandsSeen = 0;

  constructor(readonly sensor: FakeSensor, id: string, mtu: number) {
    this.id = id;
    this.mtu = mtu;
  }

  async readDeviceInfo(): Promise<DeviceInfo> {
    this.assertConnected();
    return this.sensor.deviceInfo();
  }

  async command(cmd: Command): Promise<Status> {
    this.assertConnected();
    this.commandsSeen++;
    if (this.disconnectAfterCommands !== null && this.commandsSeen > this.disconnectAfterCommands) {
      this.drop('link lost');
      throw new TransportError('disconnected');
    }
    const { status, chunks } = this.sensor.handleCommand(encodeCommand(cmd), this.mtu);
    // Round-trip through the wire form so the codec is exercised too.
    const wireStatus = decodeStatus(encodeStatus(status));
    queueMicrotask(() => {
      chunks.forEach((c, i) => {
        if (this.dropChunkIndices.has(i)) return;
        for (const h of this.chunkHandlers) h(c);
      });
    });
    return wireStatus;
  }

  onChunk(handler: (bytes: Uint8Array) => void): () => void {
    this.chunkHandlers.add(handler);
    return () => this.chunkHandlers.delete(handler);
  }

  onDisconnect(handler: (reason: string) => void): () => void {
    this.disconnectHandlers.add(handler);
    return () => this.disconnectHandlers.delete(handler);
  }

  async disconnect(): Promise<void> {
    if (this.connected) this.drop('closed by phone');
  }

  private drop(reason: string): void {
    this.connected = false;
    this.sensor.onDisconnect();
    for (const h of this.disconnectHandlers) h(reason);
  }

  private assertConnected(): void {
    if (!this.connected) throw new TransportError('not connected');
  }
}

export class FakeTransport implements SensorTransport {
  private sensors = new Map<string, FakeSensor>();
  private mtu: number;
  private peripheralId: (s: FakeSensor) => string;
  /** Test hook: ids that refuse connections. */
  unreachable = new Set<string>();
  /** Every link handed out, newest last. */
  links: FakeLink[] = [];

  constructor(sensors: FakeSensor[] = [], opts: FakeTransportOptions = {}) {
    this.mtu = opts.mtu ?? 512;
    this.peripheralId = opts.peripheralId ?? ((s) => s.deviceId);
    for (const s of sensors) this.add(s);
  }

  add(sensor: FakeSensor): void {
    this.sensors.set(this.peripheralId(sensor), sensor);
  }

  async scan(_durationMs: number, onFound: (a: SensorAdvertisement) => void): Promise<void> {
    for (const [id, s] of this.sensors) {
      onFound({ id, name: `TLM-${s.deviceId.slice(-6).toUpperCase()}`, rssi: -60, advertising: s.advertising() });
    }
  }

  async connect(id: string): Promise<SensorLink> {
    const sensor = this.sensors.get(id);
    if (!sensor || this.unreachable.has(id)) throw new TransportError(`cannot reach ${id}`);
    const link = new FakeLink(sensor, id, this.mtu);
    this.links.push(link);
    return link;
  }
}
