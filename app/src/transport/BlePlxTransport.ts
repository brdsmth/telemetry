// SensorTransport on react-native-ble-plx. Thin: bytes in, bytes out; the
// codec and the session logic live elsewhere. Needs a development build,
// not Expo Go, because ble-plx is a native module.
import { BleError, BleManager, Device, State, Subscription } from 'react-native-ble-plx';

import {
  Command,
  CONTROL_CHAR_UUID,
  DATA_CHAR_UUID,
  decodeDeviceInfo,
  decodeManufacturerData,
  decodeStatus,
  DEVICE_INFO_CHAR_UUID,
  DeviceInfo,
  encodeCommand,
  MTU,
  SERVICE_UUID,
  Status,
  STATUS_CHAR_UUID,
} from '../protocol/ble';
import { fromBase64, toBase64 } from '../protocol/bytes';
import { SensorAdvertisement, SensorLink, SensorTransport, TransportError } from './SensorTransport';

const DEFAULT_MTU = 23;

class BlePlxLink implements SensorLink {
  readonly id: string;
  readonly mtu: number;
  private chunkHandlers = new Set<(b: Uint8Array) => void>();
  private disconnectHandlers = new Set<(reason: string) => void>();
  private statusWaiters: ((s: Status) => void)[] = [];
  private subscriptions: Subscription[] = [];
  private closed = false;

  constructor(
    private readonly manager: BleManager,
    private readonly device: Device,
  ) {
    this.id = device.id;
    this.mtu = device.mtu && device.mtu > 0 ? device.mtu : DEFAULT_MTU;
  }

  async start(): Promise<void> {
    this.subscriptions.push(
      this.device.monitorCharacteristicForService(SERVICE_UUID, STATUS_CHAR_UUID, (error, characteristic) => {
        if (error) return this.fail(`status monitor: ${error.message}`);
        if (!characteristic?.value) return;
        const status = decodeStatus(fromBase64(characteristic.value));
        const waiter = this.statusWaiters.shift();
        if (waiter) waiter(status);
      }),
      this.device.monitorCharacteristicForService(SERVICE_UUID, DATA_CHAR_UUID, (error, characteristic) => {
        if (error) return this.fail(`data monitor: ${error.message}`);
        if (!characteristic?.value) return;
        const bytes = fromBase64(characteristic.value);
        for (const h of this.chunkHandlers) h(bytes);
      }),
      this.manager.onDeviceDisconnected(this.device.id, (error) => {
        this.fail(error ? error.message : 'disconnected');
      }),
    );
  }

  async readDeviceInfo(): Promise<DeviceInfo> {
    const c = await this.device.readCharacteristicForService(SERVICE_UUID, DEVICE_INFO_CHAR_UUID);
    if (!c.value) throw new TransportError('device info read returned no value');
    return decodeDeviceInfo(fromBase64(c.value));
  }

  async command(cmd: Command, timeoutMs = 5000): Promise<Status> {
    if (this.closed) throw new TransportError('not connected');
    const status = new Promise<Status>((resolve, reject) => {
      const timer = setTimeout(() => {
        const i = this.statusWaiters.indexOf(resolveWrapped);
        if (i >= 0) this.statusWaiters.splice(i, 1);
        reject(new TransportError(`no status within ${timeoutMs} ms for opcode ${cmd.opcode}`));
      }, timeoutMs);
      const resolveWrapped = (s: Status) => {
        clearTimeout(timer);
        resolve(s);
      };
      this.statusWaiters.push(resolveWrapped);
    });
    await this.device.writeCharacteristicWithResponseForService(SERVICE_UUID, CONTROL_CHAR_UUID, toBase64(encodeCommand(cmd)));
    return status;
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
    if (this.closed) return;
    this.closed = true;
    for (const s of this.subscriptions) s.remove();
    this.subscriptions = [];
    try {
      await this.manager.cancelDeviceConnection(this.device.id);
    } catch (e) {
      // Already gone is fine.
      if (!(e instanceof BleError)) throw e;
    }
  }

  private fail(reason: string): void {
    if (this.closed) return;
    this.closed = true;
    for (const s of this.subscriptions) s.remove();
    this.subscriptions = [];
    this.statusWaiters = [];
    for (const h of this.disconnectHandlers) h(reason);
  }
}

export class BlePlxTransport implements SensorTransport {
  constructor(private readonly manager: BleManager = new BleManager()) {}

  /** Resolves once the adapter is on; rejects if it is off or unauthorised. */
  async waitForPoweredOn(timeoutMs = 5000): Promise<void> {
    const state = await this.manager.state();
    if (state === State.PoweredOn) return;
    if (state === State.Unauthorized) throw new TransportError('bluetooth permission not granted');
    if (state === State.Unsupported) throw new TransportError('bluetooth not supported on this device');
    await new Promise<void>((resolve, reject) => {
      const timer = setTimeout(() => {
        sub.remove();
        reject(new TransportError(`bluetooth is ${state.toLowerCase()}`));
      }, timeoutMs);
      const sub = this.manager.onStateChange((s) => {
        if (s === State.PoweredOn) {
          clearTimeout(timer);
          sub.remove();
          resolve();
        }
      }, true);
    });
  }

  async scan(durationMs: number, onFound: (a: SensorAdvertisement) => void): Promise<void> {
    await this.waitForPoweredOn();
    const seen = new Set<string>();
    await new Promise<void>((resolve, reject) => {
      const stop = (err?: Error) => {
        clearTimeout(timer);
        this.manager.stopDeviceScan();
        err ? reject(err) : resolve();
      };
      const timer = setTimeout(() => stop(), durationMs);
      this.manager.startDeviceScan([SERVICE_UUID], { allowDuplicates: false }, (error, device) => {
        if (error) return stop(new TransportError(`scan: ${error.message}`));
        if (!device || seen.has(device.id)) return;
        seen.add(device.id);
        onFound({
          id: device.id,
          name: device.localName ?? device.name ?? null,
          rssi: device.rssi ?? null,
          advertising: device.manufacturerData ? decodeManufacturerData(fromBase64(device.manufacturerData)) : null,
        });
      });
    });
  }

  async connect(id: string, timeoutMs = 15000): Promise<SensorLink> {
    await this.waitForPoweredOn();
    let device: Device;
    try {
      device = await this.manager.connectToDevice(id, { timeout: timeoutMs });
      // Android negotiates on request; iOS ignores this and picks its own.
      try {
        device = await device.requestMTU(MTU);
      } catch {
        /* not supported on this platform */
      }
      device = await device.discoverAllServicesAndCharacteristics();
    } catch (e) {
      throw new TransportError(`connect ${id}: ${e instanceof Error ? e.message : String(e)}`);
    }
    const link = new BlePlxLink(this.manager, device);
    await link.start();
    return link;
  }

  destroy(): void {
    this.manager.destroy();
  }
}
