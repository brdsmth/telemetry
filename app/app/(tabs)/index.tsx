import ConnectedState from "@/components/bluetooth/ConnectedState";
import DisconnectedState from "@/components/bluetooth/DisconnectedState";
import { PeripheralServices } from "@/types/bluetooth";
import React, { useEffect, useState, useRef } from "react";
import { View, Text, StyleSheet, Platform, Alert, Linking, ScrollView } from "react-native";
import { BleManager, Device } from 'react-native-ble-plx';

const SECONDS_TO_SCAN_FOR = 5000; // milliseconds
const DEVICE_SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
const TRANSFER_CHARACTERISTIC_UUID = "beb5483f-36e1-4688-b7f5-ea07361b26a9";
const RECEIVE_CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";

const BluetoothDemoScreen: React.FC = () => {
  const bleManagerRef = useRef<BleManager | null>(null);
  const [isScanning, setIsScanning] = useState(false);
  const [devices, setDevices] = useState<Map<string, Device>>(new Map());
  const [connectedDevice, setConnectedDevice] = useState<Device | null>(null);
  const [bleService, setBleService] = useState<PeripheralServices | undefined>(undefined);
  const [errorLog, setErrorLog] = useState<string[]>([]);
  const [debugLog, setDebugLog] = useState<string[]>([]);

  const addDebugLog = (message: string) => {
    console.log("[DEBUG]", message);
    setDebugLog(prev => [...prev.slice(-20), `${new Date().toLocaleTimeString()}: ${message}`]);
  };

  const addErrorLog = (message: string, error?: any) => {
    const errorMsg = error ? `${message}: ${String(error)}` : message;
    console.error("[ERROR]", errorMsg);
    setErrorLog(prev => [...prev.slice(-10), `${new Date().toLocaleTimeString()}: ${errorMsg}`]);
  };

  // Helper to decode BLE base64 data to UTF-8 string
  const decodeBase64ToUTF8 = (base64String: string): string => {
    try {
      // Decode base64 to binary string
      const binaryString = atob(base64String);
      
      // For JSON (ASCII text), just clean up any non-printable characters
      let result = '';
      for (let i = 0; i < binaryString.length; i++) {
        const charCode = binaryString.charCodeAt(i);
        // Only include printable ASCII characters (32-126) plus newlines/tabs
        if ((charCode >= 32 && charCode <= 126) || charCode === 10 || charCode === 13 || charCode === 9) {
          result += binaryString.charAt(i);
        }
      }
      return result;
    } catch (error) {
      addErrorLog("Decode error", String(error));
      return `[decode failed: ${base64String.substring(0, 20)}]`;
    }
  };

  useEffect(() => {
    addDebugLog("Initializing BLE Manager...");
    
    const manager = new BleManager();
    bleManagerRef.current = manager;

    // Check initial Bluetooth state
    manager.state().then(state => {
      addDebugLog(`Initial Bluetooth state: ${state}`);
    });

    // Monitor state changes
    const subscription = manager.onStateChange((state) => {
      addDebugLog(`Bluetooth state changed: ${state}`);
    }, true);

    addDebugLog("BLE Manager initialized successfully");

    return () => {
      addDebugLog("Cleaning up BLE Manager...");
      subscription.remove();
      manager.destroy();
    };
  }, []);

  const startScan = () => {
    if (!bleManagerRef.current) {
      addErrorLog("BLE Manager not initialized");
      return;
    }

    if (isScanning) {
      addDebugLog("Already scanning, ignoring request");
      return;
    }

    addDebugLog("Start scan requested");
    setDevices(new Map());
    setIsScanning(true);

    bleManagerRef.current.startDeviceScan(
      null, // scan for all devices
      { allowDuplicates: false },
      (error, device) => {
        if (error) {
          addErrorLog("Scan error", error.message);
          setIsScanning(false);
          
          if (error.message.includes("not authorized") || error.message.includes("unauthorized")) {
            Alert.alert(
              "Bluetooth Permission Required",
              "Please grant Bluetooth permission in Settings",
              [
                { text: "Cancel", style: "cancel" },
                { text: "Open Settings", onPress: () => Linking.openURL("app-settings:") }
              ]
            );
          }
          return;
        }

        if (device && device.name) {
          addDebugLog(`Found device: ${device.name} (${device.id})`);
          setDevices(prev => new Map(prev).set(device.id, device));
        }
      }
    );

    // Stop scan after timeout
    setTimeout(() => {
      if (bleManagerRef.current && isScanning) {
        bleManagerRef.current.stopDeviceScan();
        setIsScanning(false);
        addDebugLog("Scan stopped");
      }
    }, SECONDS_TO_SCAN_FOR);

    addDebugLog(`Scanning for ${SECONDS_TO_SCAN_FOR / 1000} seconds...`);
  };

  const connectPeripheral = async (device: { id: string; name?: string }) => {
    if (!bleManagerRef.current) {
      addErrorLog("BLE Manager not initialized");
      return;
    }

    try {
      addDebugLog(`Connecting to ${device.name || device.id}...`);
      
      const connectedDevice = await bleManagerRef.current.connectToDevice(device.id);
      
      // Request larger MTU for bigger packets
      const mtu = await connectedDevice.requestMTU(512);
      addDebugLog(`MTU negotiated: ${mtu} bytes`);
      
      addDebugLog(`Connected! Discovering services...`);
      
      await connectedDevice.discoverAllServicesAndCharacteristics();
      addDebugLog(`Services discovered`);
      
      const services = await connectedDevice.services();
      addDebugLog(`Found ${services.length} services`);
      
      // Set up the connected device and service info
      setConnectedDevice(connectedDevice);
      const peripheralParameters = {
        peripheralId: device.id,
        serviceId: DEVICE_SERVICE_UUID,
        transfer: TRANSFER_CHARACTERISTIC_UUID,
        receive: RECEIVE_CHARACTERISTIC_UUID,
      };
      setBleService(peripheralParameters);
      
      // Monitor notifications from the sensor
      addDebugLog("Setting up notifications...");
      connectedDevice.monitorCharacteristicForService(
        DEVICE_SERVICE_UUID,
        RECEIVE_CHARACTERISTIC_UUID,
        (error, characteristic) => {
          if (error) {
            addErrorLog("Notification error", error.message);
            return;
          }
          
          if (characteristic?.value) {
            // Debug: show raw base64 (full)
            addDebugLog(`Raw base64 (${characteristic.value.length} chars): ${characteristic.value}`);
            
            const decodedValue = decodeBase64ToUTF8(characteristic.value);
            addDebugLog(`📡 Decoded (${decodedValue.length} bytes): ${decodedValue}`);
          }
        }
      );
      
      addDebugLog("✅ Connection complete!");
      
    } catch (error: any) {
      addErrorLog("Connection failed", error.message);
      Alert.alert("Connection Failed", `Could not connect to device: ${error.message}`);
    }
  };

  const disconnectPeripheral = async (peripheralId: string) => {
    if (!bleManagerRef.current) {
      return;
    }

    try {
      addDebugLog("Disconnecting...");
      await bleManagerRef.current.cancelDeviceConnection(peripheralId);
      setBleService(undefined);
      setConnectedDevice(null);
      setDevices(new Map());
      addDebugLog("Disconnected");
    } catch (error: any) {
      addErrorLog("Disconnect failed", error.message);
    }
  };

  const write = async () => {
    if (!connectedDevice || !bleService) {
      addErrorLog("No device connected");
      return;
    }

    try {
      const message = "Hello World";
      addDebugLog(`Writing: ${message}`);
      
      // Convert UTF-8 string to base64 (reverse of decode)
      const encoder = new TextEncoder();
      const data = encoder.encode(message);
      const base64Data = btoa(String.fromCharCode(...data));
      
      await connectedDevice.writeCharacteristicWithResponseForService(
        bleService.serviceId,
        bleService.transfer,
        base64Data
      );
      
      addDebugLog("✅ Write successful");
    } catch (error: any) {
      addErrorLog("Write failed", error.message);
    }
  };

  const read = async () => {
    if (!connectedDevice || !bleService) {
      addErrorLog("No device connected");
      return;
    }

    try {
      addDebugLog("Reading characteristic...");
      
      const characteristic = await connectedDevice.readCharacteristicForService(
        bleService.serviceId,
        bleService.receive
      );
      
      if (characteristic.value) {
        const decodedValue = decodeBase64ToUTF8(characteristic.value);
        addDebugLog(`Read value: ${decodedValue}`);
        return decodedValue;
      }
    } catch (error: any) {
      addErrorLog("Read failed", error.message);
    }
  };

  // Convert Device objects to StrippedPeripheral format
  const deviceList = Array.from(devices.values()).map(device => ({
    id: device.id,
    name: device.name || device.localName || "Unknown Device",
    localName: device.localName || undefined,
    rssi: device.rssi || 0,
  }));

  return (
    <ScrollView style={styles.container}>
      <Text style={styles.header}>Bluetooth Demo</Text>
      
      {!connectedDevice ? (
        <DisconnectedState
          peripherals={deviceList}
          isScanning={isScanning}
          onScanPress={startScan}
          onConnect={connectPeripheral}
        />
      ) : (
        bleService && (
          <ConnectedState
            onRead={read}
            onWrite={write}
            bleService={bleService}
            onDisconnect={disconnectPeripheral}
          />
        )
      )}

      {/* Error Log */}
      {errorLog.length > 0 && (
        <View style={styles.logContainer}>
          <Text style={styles.logHeader}>❌ Errors:</Text>
          {errorLog.map((log, index) => (
            <Text key={index} style={styles.errorText}>{log}</Text>
          ))}
        </View>
      )}

      {/* Debug Log */}
      {debugLog.length > 0 && (
        <View style={styles.logContainer}>
          <Text style={styles.logHeader}>🔍 Debug:</Text>
          {debugLog.slice(-5).map((log, index) => (
            <Text key={index} style={styles.debugText}>{log}</Text>
          ))}
        </View>
      )}
    </ScrollView>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: "#f5f5f5",
    paddingVertical: "10%",
    paddingHorizontal: 20,
  },
  header: {
    fontSize: 24,
    fontWeight: "bold",
    marginBottom: 16,
    color: "#333",
  },
  logContainer: {
    backgroundColor: "#fff",
    padding: 10,
    marginTop: 10,
    borderRadius: 8,
    borderWidth: 1,
    borderColor: "#ddd",
  },
  logHeader: {
    fontSize: 14,
    fontWeight: "bold",
    marginBottom: 5,
    color: "#333",
  },
  errorText: {
    fontSize: 11,
    color: "#d32f2f",
    marginVertical: 2,
    fontFamily: Platform.OS === "ios" ? "Courier" : "monospace",
  },
  debugText: {
    fontSize: 11,
    color: "#666",
    marginVertical: 2,
    fontFamily: Platform.OS === "ios" ? "Courier" : "monospace",
  },
});

export default BluetoothDemoScreen;