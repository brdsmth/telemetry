import { StyleSheet, View, ScrollView, TouchableOpacity, Switch } from 'react-native';
import { useState } from 'react';
import { ThemedText } from '@/components/themed-text';
import { ThemedView } from '@/components/themed-view';
import { IconSymbol } from '@/components/ui/icon-symbol';

export default function SettingsScreen() {
  const [notifications, setNotifications] = useState(true);
  const [autoConnect, setAutoConnect] = useState(false);
  const [darkMode, setDarkMode] = useState(false);

  return (
    <ScrollView style={styles.container}>
      <View style={styles.header}>
        <ThemedText type="title" style={styles.title}>Settings</ThemedText>
      </View>

      {/* App Settings */}
      <View style={styles.section}>
        <ThemedText style={styles.sectionTitle}>General</ThemedText>
        
        <View style={styles.settingRow}>
          <View style={styles.settingInfo}>
            <IconSymbol name="bell.fill" size={24} color="#2B8A3E" style={styles.icon} />
            <View>
              <ThemedText style={styles.settingLabel}>Notifications</ThemedText>
              <ThemedText style={styles.settingDescription}>
                Alert when moisture drops below threshold
              </ThemedText>
            </View>
          </View>
          <Switch
            value={notifications}
            onValueChange={setNotifications}
            trackColor={{ false: '#E9ECEF', true: '#51CF66' }}
            thumbColor="#fff"
          />
        </View>

        <View style={styles.settingRow}>
          <View style={styles.settingInfo}>
            <IconSymbol name="link" size={24} color="#2B8A3E" style={styles.icon} />
            <View>
              <ThemedText style={styles.settingLabel}>Auto-Connect</ThemedText>
              <ThemedText style={styles.settingDescription}>
                Connect to saved probes automatically
              </ThemedText>
            </View>
          </View>
          <Switch
            value={autoConnect}
            onValueChange={setAutoConnect}
            trackColor={{ false: '#E9ECEF', true: '#51CF66' }}
            thumbColor="#fff"
          />
        </View>

        <View style={styles.settingRow}>
          <View style={styles.settingInfo}>
            <IconSymbol name="moon.fill" size={24} color="#2B8A3E" style={styles.icon} />
            <View>
              <ThemedText style={styles.settingLabel}>Dark Mode</ThemedText>
              <ThemedText style={styles.settingDescription}>
                Use dark theme
              </ThemedText>
            </View>
          </View>
          <Switch
            value={darkMode}
            onValueChange={setDarkMode}
            trackColor={{ false: '#E9ECEF', true: '#51CF66' }}
            thumbColor="#fff"
          />
        </View>
      </View>

      {/* Moisture Thresholds */}
      <View style={styles.section}>
        <ThemedText style={styles.sectionTitle}>Moisture Thresholds</ThemedText>
        <View style={styles.infoCard}>
          <View style={styles.thresholdRow}>
            <ThemedText style={styles.thresholdLabel}>🔴 Dry</ThemedText>
            <ThemedText style={styles.thresholdValue}>Below 30%</ThemedText>
          </View>
          <View style={styles.thresholdRow}>
            <ThemedText style={styles.thresholdLabel}>🟢 Optimal</ThemedText>
            <ThemedText style={styles.thresholdValue}>30% - 60%</ThemedText>
          </View>
          <View style={styles.thresholdRow}>
            <ThemedText style={styles.thresholdLabel}>💧 Wet</ThemedText>
            <ThemedText style={styles.thresholdValue}>Above 60%</ThemedText>
          </View>
        </View>
      </View>

      {/* About */}
      <View style={styles.section}>
        <ThemedText style={styles.sectionTitle}>About</ThemedText>
        <TouchableOpacity style={styles.infoCard}>
          <ThemedText style={styles.infoLabel}>Version</ThemedText>
          <ThemedText style={styles.infoValue}>1.0.0</ThemedText>
        </TouchableOpacity>
        <TouchableOpacity style={styles.infoCard}>
          <ThemedText style={styles.infoLabel}>Support</ThemedText>
          <ThemedText style={styles.infoValue}>help@soilmonitor.app</ThemedText>
        </TouchableOpacity>
      </View>

      <View style={styles.footer}>
        <ThemedText style={styles.footerText}>
          Soil Moisture Monitoring System
        </ThemedText>
        <ThemedText style={styles.footerText}>
          Built for farmers, by farmers
        </ThemedText>
      </View>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#F8F9FA',
  },
  header: {
    paddingHorizontal: 20,
    paddingTop: 60,
    paddingBottom: 20,
    backgroundColor: '#fff',
    borderBottomWidth: 1,
    borderBottomColor: '#E9ECEF',
  },
  title: {
    fontSize: 28,
    fontWeight: '700',
    color: '#2B8A3E',
  },
  section: {
    marginTop: 24,
    paddingHorizontal: 20,
  },
  sectionTitle: {
    fontSize: 16,
    fontWeight: '600',
    color: '#868E96',
    marginBottom: 12,
    textTransform: 'uppercase',
    letterSpacing: 0.5,
  },
  settingRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    backgroundColor: '#fff',
    padding: 16,
    borderRadius: 12,
    marginBottom: 8,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.05,
    shadowRadius: 2,
    elevation: 2,
  },
  settingInfo: {
    flexDirection: 'row',
    alignItems: 'center',
    flex: 1,
    gap: 12,
  },
  icon: {
    width: 24,
  },
  settingLabel: {
    fontSize: 16,
    fontWeight: '600',
    color: '#212529',
    marginBottom: 2,
  },
  settingDescription: {
    fontSize: 13,
    color: '#868E96',
  },
  infoCard: {
    backgroundColor: '#fff',
    padding: 16,
    borderRadius: 12,
    marginBottom: 8,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.05,
    shadowRadius: 2,
    elevation: 2,
  },
  thresholdRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    paddingVertical: 8,
  },
  thresholdLabel: {
    fontSize: 15,
    color: '#212529',
  },
  thresholdValue: {
    fontSize: 15,
    fontWeight: '600',
    color: '#495057',
  },
  infoLabel: {
    fontSize: 15,
    color: '#868E96',
    marginBottom: 4,
  },
  infoValue: {
    fontSize: 16,
    fontWeight: '600',
    color: '#212529',
  },
  footer: {
    alignItems: 'center',
    paddingVertical: 40,
    paddingHorizontal: 20,
  },
  footerText: {
    fontSize: 13,
    color: '#ADB5BD',
    textAlign: 'center',
    lineHeight: 20,
  },
});
