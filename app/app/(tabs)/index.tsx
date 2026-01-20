import ConnectedState from "@/components/bluetooth/ConnectedState";
import DisconnectedState from "@/components/bluetooth/DisconnectedState";
import SensorDashboard from "@/components/dashboard/SensorDashboard";
import { PeripheralServices } from "@/types/bluetooth";
import React, { useEffect, useState, useRef } from "react";
import { View, Text, StyleSheet, Platform, Alert, Linking, ScrollView, TouchableOpacity, Modal } from "react-native";
import { BleManager, Device } from 'react-native-ble-plx';

const SECONDS_TO_SCAN_FOR = 5000; // milliseconds
const DEVICE_SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
const TRANSFER_CHARACTERISTIC_UUID = "beb5483f-36e1-4688-b7f5-ea07361b26a9";
const RECEIVE_CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";

type SensorReading = {
  id: string;
  name: string;
  moisture?: number;
  temperature?: number;
  battery?: number;
  lastUpdate: Date;
  location?: string;
};

const BluetoothDemoScreen: React.FC = () => {
  const bleManagerRef = useRef<BleManager | null>(null);
  const disconnectSubscriptionRef = useRef<any>(null);
  const [isScanning, setIsScanning] = useState(false);
  const [devices, setDevices] = useState<Map<string, Device>>(new Map());
  const [connectedDevice, setConnectedDevice] = useState<Device | null>(null);
  const [bleService, setBleService] = useState<PeripheralServices | undefined>(undefined);
  const [errorLog, setErrorLog] = useState<string[]>([]);
  const [debugLog, setDebugLog] = useState<string[]>([]);
  const [sensorReadings, setSensorReadings] = useState<SensorReading[]>([]);
  const [showConnectionModal, setShowConnectionModal] = useState(false);
  const [viewMode, setViewMode] = useState<'dashboard' | 'debug'>('dashboard');
  const [bleAvailable, setBleAvailable] = useState(true);
  const [bleError, setBleError] = useState<string | null>(null);

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
    // Check if we're on a supported platform
    if (Platform.OS === 'web') {
      const errorMsg = "Bluetooth is not supported on web. Please use iOS or Android.";
      addErrorLog(errorMsg);
      setBleAvailable(false);
      setBleError(errorMsg);
      return;
    }

    addDebugLog("Initializing BLE Manager...");
    
    try {
      const manager = new BleManager();
      bleManagerRef.current = manager;

      // Check initial Bluetooth state
      manager.state().then(state => {
        addDebugLog(`Initial Bluetooth state: ${state}`);
        if (state === 'PoweredOff') {
          setBleError("Bluetooth is turned off. Please enable Bluetooth in your device settings.");
        } else if (state === 'Unauthorized') {
          setBleError("Bluetooth permission not granted. Please enable Bluetooth permission in your device settings.");
        }
      }).catch(error => {
        addErrorLog("Failed to check Bluetooth state", error);
        setBleError("Unable to access Bluetooth. Please check your device settings.");
      });

      // Monitor state changes
      const stateSubscription = manager.onStateChange((state) => {
        addDebugLog(`Bluetooth state changed: ${state}`);
        
        // Clear error when Bluetooth becomes available
        if (state === 'PoweredOn') {
          setBleError(null);
        } else if (state === 'PoweredOff') {
          setBleError("Bluetooth is turned off. Please enable Bluetooth in your device settings.");
        } else if (state === 'Unauthorized') {
          setBleError("Bluetooth permission not granted. Please enable Bluetooth permission in your device settings.");
        }
      }, true);

      addDebugLog("BLE Manager initialized successfully");

      return () => {
        addDebugLog("Cleaning up BLE Manager...");
        try {
          stateSubscription.remove();
          if (disconnectSubscriptionRef.current) {
            disconnectSubscriptionRef.current.remove();
          }
          manager.destroy();
        } catch (cleanupError) {
          addErrorLog("Error during cleanup", cleanupError);
        }
      };
    } catch (error: any) {
      const errorMsg = `Failed to initialize Bluetooth: ${error.message || 'Unknown error'}`;
      addErrorLog(errorMsg);
      setBleAvailable(false);
      setBleError(errorMsg);
    }
  }, []);

  const startScan = () => {
    if (!bleAvailable) {
      Alert.alert("Bluetooth Unavailable", bleError || "Bluetooth is not available on this device.");
      return;
    }

    if (!bleManagerRef.current) {
      addErrorLog("BLE Manager not initialized");
      Alert.alert("Error", "Bluetooth is not ready. Please restart the app.");
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
    if (!bleAvailable || !bleManagerRef.current) {
      addErrorLog("BLE Manager not initialized or unavailable");
      Alert.alert("Error", "Bluetooth is not available. Please check your device settings and restart the app.");
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
            
            // Try to parse as sensor data JSON
            try {
              const data = JSON.parse(decodedValue);
              if (data.moisture !== undefined || data.temperature !== undefined) {
                setSensorReadings(prev => {
                  const existing = prev.find(s => s.id === device.id);
                  const newReading: SensorReading = {
                    id: device.id,
                    name: device.name || "Soil Probe",
                    moisture: data.moisture,
                    temperature: data.temperature,
                    battery: data.battery,
                    lastUpdate: new Date(),
                    location: data.location,
                  };
                  
                  if (existing) {
                    return prev.map(s => s.id === device.id ? newReading : s);
                  }
                  return [...prev, newReading];
                });
              }
            } catch (parseError) {
              // Not JSON sensor data, ignore
            }
          }
        }
      );
      
      // Monitor device disconnection
      if (disconnectSubscriptionRef.current) {
        disconnectSubscriptionRef.current.remove();
      }
      disconnectSubscriptionRef.current = bleManagerRef.current.onDeviceDisconnected(
        device.id,
        (error, disconnectedDevice) => {
          if (error) {
            addErrorLog("Disconnection error", error.message);
          }
          
          if (disconnectedDevice) {
            addDebugLog(`Device ${disconnectedDevice.name || disconnectedDevice.id} disconnected`);
            
            // Clean up connection state
            addDebugLog("Cleaning up after disconnection...");
            setConnectedDevice(null);
            setBleService(undefined);
            setDevices(new Map());
            
            Alert.alert(
              "Device Disconnected",
              `${disconnectedDevice.name || "ESP32"} has disconnected.`,
              [{ text: "OK" }]
            );
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
      
      // Stop any ongoing scan
      if (isScanning) {
        bleManagerRef.current.stopDeviceScan();
        setIsScanning(false);
      }
      
      // Disconnect device
      await bleManagerRef.current.cancelDeviceConnection(peripheralId);
      
      // Reset all connection state
      setBleService(undefined);
      setConnectedDevice(null);
      setDevices(new Map());
      setSensorReadings(prev => prev.filter(s => s.id !== peripheralId));
      
      // Clear logs for fresh start
      setErrorLog([]);
      setDebugLog([]);
      
      addDebugLog("✅ Disconnected successfully - ready to scan again");
      
    } catch (error: any) {
      // Even if disconnect fails, reset the state
      addErrorLog("Disconnect error (state reset anyway)", error.message);
      setBleService(undefined);
      setConnectedDevice(null);
      setDevices(new Map());
      setSensorReadings(prev => prev.filter(s => s.id !== peripheralId));
    }
  };

  const write = async () => {
    if (!bleAvailable) {
      addErrorLog("Bluetooth not available");
      return;
    }

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
    if (!bleAvailable) {
      addErrorLog("Bluetooth not available");
      return;
    }

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

  const forceReset = async () => {
    addDebugLog("🔄 Force reset initiated");
    
    if (bleManagerRef.current) {
      try {
        // Stop scan if running
        if (isScanning) {
          bleManagerRef.current.stopDeviceScan();
        }
        
        // Disconnect if connected
        if (connectedDevice) {
          await bleManagerRef.current.cancelDeviceConnection(connectedDevice.id).catch(() => {});
        }
      } catch (error) {
        // Ignore errors during force reset
      }
    }
    
    // Reset all state
    setIsScanning(false);
    setDevices(new Map());
    setConnectedDevice(null);
    setBleService(undefined);
    setErrorLog([]);
    setDebugLog([]);
    setSensorReadings([]);
    
    addDebugLog("✅ Force reset complete - ready to scan");
    
    Alert.alert("Reset Complete", "All connections cleared. You can now scan for devices.");
  };

  const handleAddSensor = () => {
    setShowConnectionModal(true);
  };

  const handleCloseModal = () => {
    setShowConnectionModal(false);
    // Stop scanning when closing modal
    if (isScanning && bleManagerRef.current) {
      bleManagerRef.current.stopDeviceScan();
      setIsScanning(false);
    }
  };

  return (
    <View style={styles.mainContainer}>
      {/* BLE Error Banner */}
      {bleError && (
        <View style={styles.errorBanner}>
          <Text style={styles.errorBannerText}>⚠️ {bleError}</Text>
        </View>
      )}

      {/* Main Dashboard View */}
      <SensorDashboard
        sensors={sensorReadings}
        onAddSensor={handleAddSensor}
        isBluetoothAvailable={bleAvailable}
        onRefresh={() => {
          // Trigger a read from all connected devices
          if (connectedDevice && bleService) {
            read();
          }
        }}
      />

      {/* Debug Toggle Button */}
      <TouchableOpacity
        style={styles.debugToggle}
        onPress={() => setViewMode(viewMode === 'dashboard' ? 'debug' : 'dashboard')}
      >
        <Text style={styles.debugToggleText}>
          {viewMode === 'dashboard' ? '🐛' : '📊'}
        </Text>
      </TouchableOpacity>

      {/* Connection Modal */}
      <Modal
        visible={showConnectionModal}
        animationType="slide"
        presentationStyle="pageSheet"
        onRequestClose={handleCloseModal}
      >
        <View style={styles.modalContainer}>
          <View style={styles.modalHeader}>
            <Text style={styles.modalTitle}>Connect Soil Probe</Text>
            <TouchableOpacity onPress={handleCloseModal} style={styles.closeButton}>
              <Text style={styles.closeButtonText}>✕</Text>
            </TouchableOpacity>
          </View>

          <ScrollView style={styles.modalContent}>
            {!bleAvailable ? (
              <View style={styles.bleUnavailableContainer}>
                <Text style={styles.bleUnavailableEmoji}>📱</Text>
                <Text style={styles.bleUnavailableTitle}>Bluetooth Not Available</Text>
                <Text style={styles.bleUnavailableText}>{bleError}</Text>
                {Platform.OS === 'web' && (
                  <Text style={styles.bleUnavailableHint}>
                    Please use the mobile app on iOS or Android to connect to soil probes.
                  </Text>
                )}
              </View>
            ) : !connectedDevice ? (
              <DisconnectedState
                peripherals={deviceList}
                isScanning={isScanning}
                onScanPress={startScan}
                onConnect={async (device) => {
                  await connectPeripheral(device);
                  // Close modal after successful connection
                  setTimeout(() => setShowConnectionModal(false), 1000);
                }}
              />
            ) : (
              bleService && (
                <View>
                  <View style={styles.connectedBanner}>
                    <Text style={styles.connectedBannerText}>
                      ✓ Connected to {connectedDevice.name}
                    </Text>
                  </View>
                  <ConnectedState
                    onRead={read}
                    onWrite={write}
                    bleService={bleService}
                    onDisconnect={(id) => {
                      disconnectPeripheral(id);
                      setShowConnectionModal(false);
                    }}
                  />
                </View>
              )
            )}
          </ScrollView>
        </View>
      </Modal>

      {/* Debug Modal */}
      <Modal
        visible={viewMode === 'debug'}
        animationType="slide"
        presentationStyle="pageSheet"
        onRequestClose={() => setViewMode('dashboard')}
      >
        <View style={styles.modalContainer}>
          <View style={styles.modalHeader}>
            <Text style={styles.modalTitle}>Debug Console</Text>
            <TouchableOpacity
              onPress={() => setViewMode('dashboard')}
              style={styles.closeButton}
            >
              <Text style={styles.closeButtonText}>✕</Text>
            </TouchableOpacity>
          </View>

          <ScrollView style={styles.debugContainer}>
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
                {debugLog.map((log, index) => (
                  <Text key={index} style={styles.debugText}>{log}</Text>
                ))}
              </View>
            )}

            {/* Force Reset Button */}
            <TouchableOpacity style={styles.forceResetButton} onPress={forceReset}>
              <Text style={styles.forceResetButtonText}>🔄 Force Reset All</Text>
            </TouchableOpacity>
          </ScrollView>
        </View>
      </Modal>
    </View>
  );
};

const styles = StyleSheet.create({
  mainContainer: {
    flex: 1,
    backgroundColor: "#F8F9FA",
  },
  errorBanner: {
    backgroundColor: "#FFF3CD",
    borderBottomWidth: 1,
    borderBottomColor: "#FFE69C",
    paddingHorizontal: 16,
    paddingVertical: 12,
    paddingTop: 50,
  },
  errorBannerText: {
    color: "#856404",
    fontSize: 14,
    textAlign: "center",
    lineHeight: 20,
  },
  debugToggle: {
    position: "absolute",
    bottom: 30,
    right: 20,
    width: 56,
    height: 56,
    borderRadius: 28,
    backgroundColor: "#2B8A3E",
    justifyContent: "center",
    alignItems: "center",
    shadowColor: "#000",
    shadowOffset: { width: 0, height: 4 },
    shadowOpacity: 0.3,
    shadowRadius: 8,
    elevation: 8,
  },
  debugToggleText: {
    fontSize: 24,
  },
  modalContainer: {
    flex: 1,
    backgroundColor: "#fff",
  },
  modalHeader: {
    flexDirection: "row",
    justifyContent: "space-between",
    alignItems: "center",
    paddingHorizontal: 20,
    paddingTop: 60,
    paddingBottom: 20,
    borderBottomWidth: 1,
    borderBottomColor: "#E9ECEF",
  },
  modalTitle: {
    fontSize: 24,
    fontWeight: "700",
    color: "#212529",
  },
  closeButton: {
    width: 36,
    height: 36,
    borderRadius: 18,
    backgroundColor: "#F1F3F5",
    justifyContent: "center",
    alignItems: "center",
  },
  closeButtonText: {
    fontSize: 20,
    color: "#495057",
    fontWeight: "600",
  },
  modalContent: {
    flex: 1,
    padding: 20,
  },
  connectedBanner: {
    backgroundColor: "#D3F9D8",
    padding: 16,
    borderRadius: 8,
    marginBottom: 16,
  },
  connectedBannerText: {
    color: "#2B8A3E",
    fontSize: 16,
    fontWeight: "600",
    textAlign: "center",
  },
  debugContainer: {
    flex: 1,
    padding: 20,
  },
  logContainer: {
    backgroundColor: "#F8F9FA",
    padding: 16,
    marginBottom: 16,
    borderRadius: 8,
    borderWidth: 1,
    borderColor: "#E9ECEF",
  },
  logHeader: {
    fontSize: 16,
    fontWeight: "700",
    marginBottom: 8,
    color: "#212529",
  },
  errorText: {
    fontSize: 12,
    color: "#FA5252",
    marginVertical: 2,
    fontFamily: Platform.OS === "ios" ? "Courier" : "monospace",
    lineHeight: 18,
  },
  debugText: {
    fontSize: 12,
    color: "#495057",
    marginVertical: 2,
    fontFamily: Platform.OS === "ios" ? "Courier" : "monospace",
    lineHeight: 18,
  },
  forceResetButton: {
    backgroundColor: "#FA5252",
    padding: 16,
    borderRadius: 8,
    alignItems: "center",
    marginTop: 8,
  },
  forceResetButtonText: {
    color: "#fff",
    fontSize: 16,
    fontWeight: "600",
  },
  bleUnavailableContainer: {
    alignItems: "center",
    justifyContent: "center",
    paddingVertical: 60,
    paddingHorizontal: 40,
  },
  bleUnavailableEmoji: {
    fontSize: 64,
    marginBottom: 16,
  },
  bleUnavailableTitle: {
    fontSize: 20,
    fontWeight: "600",
    color: "#212529",
    marginBottom: 8,
  },
  bleUnavailableText: {
    fontSize: 15,
    color: "#868E96",
    textAlign: "center",
    marginBottom: 16,
    lineHeight: 22,
  },
  bleUnavailableHint: {
    fontSize: 13,
    color: "#ADB5BD",
    textAlign: "center",
    lineHeight: 20,
    fontStyle: "italic",
  },
});

export default BluetoothDemoScreen;