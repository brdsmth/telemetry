import { View, Text, TouchableOpacity, StyleSheet } from "react-native";
import React from "react";
import { PeripheralServices } from "../../types/bluetooth";

interface ConnectedStateProps {
  bleService: PeripheralServices;
  onRead: () => void;
  onWrite: () => void;
  onDisconnect: (id: string) => void;
}

const ConnectedState: React.FunctionComponent<ConnectedStateProps> = ({bleService,onDisconnect,onRead,onWrite}) => {
  return (
    <>
      <View style={styles.card}>
        <Text style={styles.info}>
          Peripheral ID: {bleService.peripheralId} dBm
        </Text>
        <Text style={styles.info}>Service ID: {bleService.serviceId}</Text>
      </View>

      <View style={styles.actionButtons}>
        <TouchableOpacity
          onPress={onRead}
          style={styles.button}
        >
          <Text style={styles.buttonText}>READ</Text>
        </TouchableOpacity>

        <TouchableOpacity
          onPress={onWrite}
          style={styles.button}
        >
          <Text style={styles.buttonText}>WRITE</Text>
        </TouchableOpacity>

        <TouchableOpacity
          onPress={() => onDisconnect(bleService.peripheralId)}
          style={styles.disconnectButton}
        >
          <Text style={styles.buttonText}>DISCONNECT</Text>
        </TouchableOpacity>
      </View>
    </>
  );
};

export default ConnectedState;


const styles = StyleSheet.create({
    actionButtons: {
      flexDirection: "row",
      marginTop: 16,
    },
    button: {
      backgroundColor: "#007AFF",
      padding: 12,
      borderRadius: 8,
      marginHorizontal: 8,
      flexGrow: 1,
    },
    disconnectButton: {
      backgroundColor: "red",
      padding: 12,
      borderRadius: 8,
      marginHorizontal: 8,
    },
    buttonText: {
      color: "#fff",
      fontSize: 16,
      textAlign: "center",
      fontWeight: "500",
    },
    info: {
      fontSize: 14,
      color: "#333",
    },
    card: {
      backgroundColor: "#fff",
      padding: 16,
      marginVertical: 8,
      borderRadius: 10,
      shadowColor: "#000",
      shadowOffset: { width: 0, height: 2 },
      shadowOpacity: 0.1,
      shadowRadius: 4,
      elevation: 3,
    },
  });