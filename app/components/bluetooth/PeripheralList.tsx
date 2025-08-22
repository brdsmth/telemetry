import { StrippedPeripheral } from "../../types/bluetooth";
import React from "react";
import {
  View,
  FlatList,
  StyleSheet,
  Text,
  TouchableOpacity,
} from "react-native";

interface PeripheralListProps {
  peripherals: StrippedPeripheral[];
  onConnect: (peripheral: StrippedPeripheral) => Promise<void>;
}

const PeripheralList: React.FC<PeripheralListProps> = ({
  peripherals,
  onConnect,
}) => {
  return (
    <View style={styles.container}>
      <FlatList
        data={peripherals}
        keyExtractor={(item) => item.id}
        renderItem={({ item }) => (
          <TouchableOpacity onPress={() => onConnect(item)} style={styles.card}>
            <Text style={styles.title}>{item.name ?? "Unknown Device"}</Text>
            <Text style={styles.subtitle}>
              Local Name: {item.localName ?? "N/A"}
            </Text>
            <Text style={styles.info}>RSSI: {item.rssi} dBm</Text>
            <Text style={styles.info}>ID: {item.id}</Text>
          </TouchableOpacity>
        )}
      />
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
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
  title: {
    fontSize: 18,
    fontWeight: "bold",
    marginBottom: 4,
  },
  subtitle: {
    fontSize: 14,
    color: "#555",
    marginBottom: 4,
  },
  info: {
    fontSize: 14,
    color: "#333",
  },
});

export default PeripheralList;