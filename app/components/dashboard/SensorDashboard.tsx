import React from "react";
import { View, Text, StyleSheet, ScrollView, TouchableOpacity } from "react-native";

type SensorReading = {
  id: string;
  name: string;
  moisture?: number;
  temperature?: number;
  battery?: number;
  lastUpdate: Date;
  location?: string;
};

interface SensorDashboardProps {
  sensors: SensorReading[];
  onAddSensor: () => void;
  onRefresh?: () => void;
  isBluetoothAvailable?: boolean;
}

const getMoistureStatus = (moisture: number) => {
  if (moisture < 30) return { status: "Dry", color: "#FF6B6B", emoji: "🔴" };
  if (moisture < 60) return { status: "Optimal", color: "#51CF66", emoji: "🟢" };
  return { status: "Wet", color: "#4DABF7", emoji: "💧" };
};

const SensorDashboard: React.FC<SensorDashboardProps> = ({
  sensors,
  onAddSensor,
  onRefresh,
  isBluetoothAvailable = true,
}) => {
  return (
    <View style={styles.container}>
      {/* Header */}
      <View style={styles.header}>
        <View>
          <Text style={styles.headerTitle}>Soil Monitor</Text>
          <Text style={styles.headerSubtitle}>
            {sensors.length} {sensors.length === 1 ? "Probe" : "Probes"} Connected
          </Text>
        </View>
        {onRefresh && (
          <TouchableOpacity onPress={onRefresh} style={styles.refreshButton}>
            <Text style={styles.refreshButtonText}>↻</Text>
          </TouchableOpacity>
        )}
      </View>

      <ScrollView style={styles.scrollView} showsVerticalScrollIndicator={false}>
        {/* Summary Cards */}
        {sensors.length > 0 && (
          <View style={styles.summaryContainer}>
            <View style={styles.summaryCard}>
              <Text style={styles.summaryValue}>
                {Math.round(
                  sensors.reduce((acc, s) => acc + (s.moisture || 0), 0) /
                    sensors.length
                )}%
              </Text>
              <Text style={styles.summaryLabel}>Avg Moisture</Text>
            </View>
            <View style={styles.summaryCard}>
              <Text style={styles.summaryValue}>
                {sensors.filter((s) => (s.moisture || 0) < 30).length}
              </Text>
              <Text style={styles.summaryLabel}>Need Water</Text>
            </View>
            <View style={styles.summaryCard}>
              <Text style={styles.summaryValue}>
                {sensors.filter((s) => s.battery && s.battery < 20).length}
              </Text>
              <Text style={styles.summaryLabel}>Low Battery</Text>
            </View>
          </View>
        )}

        {/* Sensor Cards */}
        {sensors.length === 0 ? (
          <View style={styles.emptyState}>
            <Text style={styles.emptyStateEmoji}>📡</Text>
            <Text style={styles.emptyStateTitle}>No Sensors Connected</Text>
            <Text style={styles.emptyStateText}>
              Connect your soil moisture probes to start monitoring
            </Text>
            <TouchableOpacity style={styles.addButton} onPress={onAddSensor}>
              <Text style={styles.addButtonText}>+ Connect Probe</Text>
            </TouchableOpacity>
          </View>
        ) : (
          <>
            {sensors.map((sensor) => {
              const moistureInfo = sensor.moisture
                ? getMoistureStatus(sensor.moisture)
                : null;

              return (
                <View key={sensor.id} style={styles.sensorCard}>
                  <View style={styles.sensorHeader}>
                    <View style={styles.sensorTitleRow}>
                      <Text style={styles.sensorName}>{sensor.name}</Text>
                      {moistureInfo && (
                        <View style={styles.statusBadge}>
                          <Text style={styles.statusEmoji}>
                            {moistureInfo.emoji}
                          </Text>
                          <Text
                            style={[
                              styles.statusText,
                              { color: moistureInfo.color },
                            ]}
                          >
                            {moistureInfo.status}
                          </Text>
                        </View>
                      )}
                    </View>
                    {sensor.location && (
                      <Text style={styles.sensorLocation}>📍 {sensor.location}</Text>
                    )}
                  </View>

                  <View style={styles.metricsContainer}>
                    {/* Moisture */}
                    {sensor.moisture !== undefined && (
                      <View style={styles.metricCard}>
                        <Text style={styles.metricValue}>{sensor.moisture}%</Text>
                        <Text style={styles.metricLabel}>Moisture</Text>
                        <View style={styles.progressBar}>
                          <View
                            style={[
                              styles.progressFill,
                              {
                                width: `${sensor.moisture}%`,
                                backgroundColor: moistureInfo?.color || "#ccc",
                              },
                            ]}
                          />
                        </View>
                      </View>
                    )}

                    {/* Temperature */}
                    {sensor.temperature !== undefined && (
                      <View style={styles.metricCard}>
                        <Text style={styles.metricValue}>
                          {sensor.temperature}°C
                        </Text>
                        <Text style={styles.metricLabel}>Temperature</Text>
                      </View>
                    )}

                    {/* Battery */}
                    {sensor.battery !== undefined && (
                      <View style={styles.metricCard}>
                        <Text
                          style={[
                            styles.metricValue,
                            sensor.battery < 20 && styles.metricValueWarning,
                          ]}
                        >
                          {sensor.battery}%
                        </Text>
                        <Text style={styles.metricLabel}>Battery</Text>
                      </View>
                    )}
                  </View>

                  <Text style={styles.lastUpdate}>
                    Updated {formatTime(sensor.lastUpdate)}
                  </Text>
                </View>
              );
            })}

            <TouchableOpacity
              style={styles.addButtonSecondary}
              onPress={onAddSensor}
            >
              <Text style={styles.addButtonSecondaryText}>+ Add Another Probe</Text>
            </TouchableOpacity>
          </>
        )}
      </ScrollView>
    </View>
  );
};

const formatTime = (date: Date) => {
  const now = new Date();
  const diffMs = now.getTime() - date.getTime();
  const diffMins = Math.floor(diffMs / 60000);

  if (diffMins < 1) return "just now";
  if (diffMins < 60) return `${diffMins}m ago`;
  const diffHours = Math.floor(diffMins / 60);
  if (diffHours < 24) return `${diffHours}h ago`;
  const diffDays = Math.floor(diffHours / 24);
  return `${diffDays}d ago`;
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: "#F8F9FA",
  },
  header: {
    flexDirection: "row",
    justifyContent: "space-between",
    alignItems: "center",
    paddingHorizontal: 20,
    paddingTop: 60,
    paddingBottom: 20,
    backgroundColor: "#fff",
    borderBottomWidth: 1,
    borderBottomColor: "#E9ECEF",
  },
  headerTitle: {
    fontSize: 28,
    fontWeight: "700",
    color: "#2B8A3E",
  },
  headerSubtitle: {
    fontSize: 14,
    color: "#868E96",
    marginTop: 4,
  },
  refreshButton: {
    width: 40,
    height: 40,
    borderRadius: 20,
    backgroundColor: "#F1F3F5",
    justifyContent: "center",
    alignItems: "center",
  },
  refreshButtonText: {
    fontSize: 24,
    color: "#2B8A3E",
  },
  scrollView: {
    flex: 1,
    padding: 20,
  },
  summaryContainer: {
    flexDirection: "row",
    gap: 12,
    marginBottom: 20,
  },
  summaryCard: {
    flex: 1,
    backgroundColor: "#fff",
    padding: 16,
    borderRadius: 12,
    alignItems: "center",
    shadowColor: "#000",
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.05,
    shadowRadius: 2,
    elevation: 2,
  },
  summaryValue: {
    fontSize: 24,
    fontWeight: "700",
    color: "#212529",
    marginBottom: 4,
  },
  summaryLabel: {
    fontSize: 12,
    color: "#868E96",
    textAlign: "center",
  },
  emptyState: {
    alignItems: "center",
    justifyContent: "center",
    paddingVertical: 60,
    paddingHorizontal: 40,
  },
  emptyStateEmoji: {
    fontSize: 64,
    marginBottom: 16,
  },
  emptyStateTitle: {
    fontSize: 20,
    fontWeight: "600",
    color: "#212529",
    marginBottom: 8,
  },
  emptyStateText: {
    fontSize: 14,
    color: "#868E96",
    textAlign: "center",
    marginBottom: 24,
    lineHeight: 20,
  },
  addButton: {
    backgroundColor: "#2B8A3E",
    paddingHorizontal: 32,
    paddingVertical: 14,
    borderRadius: 8,
  },
  addButtonText: {
    color: "#fff",
    fontSize: 16,
    fontWeight: "600",
  },
  sensorCard: {
    backgroundColor: "#fff",
    borderRadius: 16,
    padding: 20,
    marginBottom: 16,
    shadowColor: "#000",
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 8,
    elevation: 3,
  },
  sensorHeader: {
    marginBottom: 16,
  },
  sensorTitleRow: {
    flexDirection: "row",
    justifyContent: "space-between",
    alignItems: "center",
    marginBottom: 4,
  },
  sensorName: {
    fontSize: 20,
    fontWeight: "600",
    color: "#212529",
  },
  statusBadge: {
    flexDirection: "row",
    alignItems: "center",
    backgroundColor: "#F8F9FA",
    paddingHorizontal: 10,
    paddingVertical: 4,
    borderRadius: 12,
    gap: 4,
  },
  statusEmoji: {
    fontSize: 12,
  },
  statusText: {
    fontSize: 13,
    fontWeight: "600",
  },
  sensorLocation: {
    fontSize: 14,
    color: "#868E96",
  },
  metricsContainer: {
    flexDirection: "row",
    gap: 12,
    marginBottom: 12,
  },
  metricCard: {
    flex: 1,
    backgroundColor: "#F8F9FA",
    padding: 12,
    borderRadius: 10,
  },
  metricValue: {
    fontSize: 22,
    fontWeight: "700",
    color: "#212529",
    marginBottom: 2,
  },
  metricValueWarning: {
    color: "#FA5252",
  },
  metricLabel: {
    fontSize: 11,
    color: "#868E96",
    textTransform: "uppercase",
    letterSpacing: 0.5,
  },
  progressBar: {
    height: 4,
    backgroundColor: "#E9ECEF",
    borderRadius: 2,
    marginTop: 8,
    overflow: "hidden",
  },
  progressFill: {
    height: "100%",
    borderRadius: 2,
  },
  lastUpdate: {
    fontSize: 12,
    color: "#ADB5BD",
    textAlign: "right",
  },
  addButtonSecondary: {
    backgroundColor: "#fff",
    padding: 16,
    borderRadius: 12,
    borderWidth: 2,
    borderColor: "#2B8A3E",
    borderStyle: "dashed",
    alignItems: "center",
    marginTop: 8,
    marginBottom: 20,
  },
  addButtonSecondaryText: {
    color: "#2B8A3E",
    fontSize: 16,
    fontWeight: "600",
  },
});

export default SensorDashboard;
