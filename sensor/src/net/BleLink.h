// BleLink.h
//
// BLE GATT server exposing the latest reading as a READ/NOTIFY characteristic.
#pragma once
#include <Arduino.h>

namespace ble_link {

void begin(const char* deviceName);

// Updates the read characteristic and notifies a connected central, if any.
void publish(const String& json);

bool isConnected();

}  // namespace ble_link
