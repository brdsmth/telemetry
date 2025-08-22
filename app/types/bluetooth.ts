export type PeripheralServices = {
  peripheralId: string;
  serviceId: string;
  transfer: string;
  receive: string;
};

export interface StrippedPeripheral {
  name?: string;
  localName?: string;
  rssi: number;
  id: string;
}