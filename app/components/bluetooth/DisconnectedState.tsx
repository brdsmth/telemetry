import { StyleSheet, Text, TouchableOpacity } from "react-native";
import React from "react";
import { StrippedPeripheral } from "../../types/bluetooth";
import PeripheralList from "./PeripheralList";

interface DisconnectedStateProps {
  peripherals: StrippedPeripheral[];
  isScanning: boolean;
  onScanPress: () => void;
  onConnect: (peripheral: StrippedPeripheral) => Promise<void>;
}

const DisconnectedState: React.FunctionComponent<DisconnectedStateProps> = ({
  isScanning,
  onScanPress,
  peripherals,
  onConnect,
}) => {
  return (
    <>
      <TouchableOpacity style={styles.scanButton} onPress={onScanPress}>
        <Text style={styles.scanButtonText}>
          {isScanning ? "Scanning..." : "Start Scan"}
        </Text>
      </TouchableOpacity>

      {peripherals.length > 0 ? (
        <PeripheralList onConnect={onConnect} peripherals={peripherals} />
      ) : (
        <Text style={styles.emptyText}>No peripherals found</Text>
      )}
    </>
  );
};

export default DisconnectedState;

const styles = StyleSheet.create({
  scanButton: {
    backgroundColor: "#007AFF",
    padding: 12,
    borderRadius: 8,
    marginBottom: 16,
  },
  scanButtonText: {
    color: "#fff",
    fontSize: 16,
    textAlign: "center",
    fontWeight: "500",
  },
  emptyText: {
    fontSize: 16,
    color: "#666",
    marginTop: 20,
  },
});