// Settings: where the server is, whether to use fake sensors, and what the
// services have been logging. Kept to what the pipeline needs.
import Constants from 'expo-constants';
import { useEffect, useState } from 'react';
import { Pressable, ScrollView, StyleSheet, Switch, Text, TextInput, View } from 'react-native';

import { API_URL_SETTING, DEFAULT_API_URL, FAKE_SENSORS_SETTING, getServices, resetServices, Services } from '@/src/platform/services';
import { PROTOCOL_VERSION } from '@/src/protocol/record';

export default function SettingsScreen() {
  const [services, setServices] = useState<Services | null>(null);
  const [apiUrl, setApiUrl] = useState(DEFAULT_API_URL);
  const [fake, setFake] = useState(false);
  const [phoneId, setPhoneId] = useState<string>('');
  const [health, setHealth] = useState<string>('unknown');
  const [saved, setSaved] = useState<string | null>(null);
  const [log, setLog] = useState<string[]>([]);

  const load = async () => {
    const s = await getServices();
    setServices(s);
    setApiUrl(s.apiUrl);
    setFake(s.usingFakeSensors);
    setPhoneId(await s.upload.phoneId());
    setLog([...s.log].reverse());
  };

  useEffect(() => {
    load().catch((e) => setSaved(String(e)));
    const t = setInterval(() => {
      if (services) setLog([...services.log].reverse());
    }, 2000);
    return () => clearInterval(t);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [services]);

  const save = async () => {
    if (!services) return;
    await services.repo.setSetting(API_URL_SETTING, apiUrl.trim().replace(/\/+$/, ''));
    await services.repo.setSetting(FAKE_SENSORS_SETTING, fake ? 'true' : 'false');
    resetServices();
    setSaved('Saved. Services restarted with the new settings.');
    await load();
  };

  const checkHealth = async () => {
    if (!services) return;
    setHealth('checking…');
    setHealth((await services.api.health()) ? 'ok' : 'unreachable');
  };

  return (
    <ScrollView style={styles.screen} contentContainerStyle={styles.content}>
      <Text style={styles.title}>Settings</Text>

      <Text style={styles.label}>API URL</Text>
      <TextInput
        style={styles.input}
        value={apiUrl}
        onChangeText={setApiUrl}
        autoCapitalize="none"
        autoCorrect={false}
        keyboardType="url"
      />
      <View style={styles.row}>
        <Pressable style={styles.button} onPress={checkHealth}>
          <Text style={styles.buttonText}>Check health</Text>
        </Pressable>
        <Text style={styles.health}>{health}</Text>
      </View>

      <View style={styles.switchRow}>
        <View style={{ flex: 1 }}>
          <Text style={styles.label}>Use fake sensors</Text>
          <Text style={styles.dim}>Two in-memory sensors that speak the protocol. For the simulator and web.</Text>
        </View>
        <Switch value={fake} onValueChange={setFake} />
      </View>

      <Pressable style={styles.buttonPrimary} onPress={save} disabled={!services}>
        <Text style={styles.buttonText}>Save</Text>
      </Pressable>
      {saved && <Text style={styles.saved}>{saved}</Text>}

      <Text style={styles.section}>About</Text>
      <Text style={styles.kv}>phone id  {phoneId}</Text>
      <Text style={styles.kv}>app       {Constants.expoConfig?.version ?? '?'}</Text>
      <Text style={styles.kv}>protocol  v{PROTOCOL_VERSION}</Text>

      <Text style={styles.section}>Log</Text>
      {log.length === 0 ? (
        <Text style={styles.dim}>Nothing yet.</Text>
      ) : (
        log.map((line, i) => (
          <Text key={i} style={styles.logLine}>
            {line}
          </Text>
        ))
      )}
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  screen: { flex: 1, backgroundColor: '#f4f5f7' },
  content: { padding: 16, paddingTop: 56, paddingBottom: 40, gap: 10 },
  title: { fontSize: 26, fontWeight: '700', color: '#1b1f24' },
  label: { fontSize: 13, fontWeight: '600', color: '#57606a' },
  input: { backgroundColor: '#fff', borderWidth: 1, borderColor: '#d0d7de', borderRadius: 8, padding: 10, fontFamily: 'Menlo', fontSize: 13 },
  row: { flexDirection: 'row', alignItems: 'center', gap: 12 },
  switchRow: { flexDirection: 'row', alignItems: 'center', gap: 12, marginTop: 8 },
  button: { backgroundColor: '#e6e8eb', paddingVertical: 10, paddingHorizontal: 14, borderRadius: 8 },
  buttonPrimary: { backgroundColor: '#1f6feb', paddingVertical: 12, borderRadius: 8, alignItems: 'center', marginTop: 8 },
  buttonText: { color: '#1b1f24', fontWeight: '600' },
  health: { color: '#57606a' },
  saved: { color: '#1a7f37' },
  section: { marginTop: 14, fontSize: 13, fontWeight: '600', color: '#6e7781', textTransform: 'uppercase', letterSpacing: 0.6 },
  kv: { fontFamily: 'Menlo', fontSize: 12, color: '#1b1f24' },
  dim: { color: '#6e7781', fontSize: 12 },
  logLine: { fontFamily: 'Menlo', fontSize: 11, color: '#57606a' },
});
