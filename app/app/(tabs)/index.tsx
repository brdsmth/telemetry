// Sensors: scan, collect, upload. Deliberately plain; the point is to see the
// pipeline work. Per sensor it shows the three counts that matter: pending on
// the sensor, collected on this phone, secured on the server.
import { useCallback, useEffect, useState } from 'react';
import { ActivityIndicator, Pressable, ScrollView, StyleSheet, Text, View } from 'react-native';

import { getServices, Services } from '@/src/platform/services';
import { DeviceRow, ReadingCounts } from '@/src/store/Repository';
import { CollectionResult } from '@/src/sync/CollectionService';
import { UploadResult } from '@/src/sync/UploadService';
import { SensorAdvertisement } from '@/src/transport/SensorTransport';

type DeviceView = DeviceRow & { counts: ReadingCounts };

function ago(unix: number | null): string {
  if (!unix) return 'never';
  const s = Math.max(0, Math.floor(Date.now() / 1000) - unix);
  if (s < 90) return `${s}s ago`;
  if (s < 5400) return `${Math.round(s / 60)}m ago`;
  if (s < 172800) return `${(s / 3600).toFixed(1)}h ago`;
  return `${Math.round(s / 86400)}d ago`;
}

export default function SensorsScreen() {
  const [services, setServices] = useState<Services | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [nearby, setNearby] = useState<SensorAdvertisement[]>([]);
  const [devices, setDevices] = useState<DeviceView[]>([]);
  const [busy, setBusy] = useState<string | null>(null);
  const [lastResult, setLastResult] = useState<string | null>(null);

  const refresh = useCallback(async (s: Services) => {
    const rows = await s.repo.listDevices();
    const views = await Promise.all(rows.map(async (d) => ({ ...d, counts: await s.repo.countReadings(d.deviceId) })));
    setDevices(views);
  }, []);

  useEffect(() => {
    getServices()
      .then(async (s) => {
        setServices(s);
        await refresh(s);
      })
      .catch((e) => setError(String(e)));
  }, [refresh]);

  const run = async (label: string, fn: () => Promise<string>) => {
    if (!services || busy) return;
    setBusy(label);
    setError(null);
    try {
      setLastResult(await fn());
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    } finally {
      await refresh(services).catch(() => undefined);
      setBusy(null);
    }
  };

  const scan = () =>
    run('Scanning', async () => {
      const found: SensorAdvertisement[] = [];
      setNearby([]);
      await services!.transport.scan(5000, (a) => {
        found.push(a);
        setNearby([...found]);
      });
      return `${found.length} sensor${found.length === 1 ? '' : 's'} in range`;
    });

  const collect = (a: SensorAdvertisement) =>
    run(`Collecting ${a.name ?? a.id}`, async () => {
      const r: CollectionResult = await services!.collection.collect(a.id);
      return `${a.name ?? a.id}: ${r.pulled} pulled (seq ${r.fromSeq}–${r.throughSeq}), ${r.gaps} gaps, ${r.pendingAfter} still on sensor` +
        (r.securedAckSent ? `, freed through ${r.securedAckSent}` : '');
    });

  const collectAll = () =>
    run('Collecting all', async () => {
      const parts: string[] = [];
      for (const a of nearby) {
        try {
          const r = await services!.collection.collect(a.id);
          parts.push(`${a.name ?? a.id}: ${r.pulled}`);
        } catch (e) {
          parts.push(`${a.name ?? a.id}: failed (${e instanceof Error ? e.message : String(e)})`);
        }
      }
      return parts.join('\n') || 'nothing nearby';
    });

  const upload = () =>
    run('Uploading', async () => {
      const r: UploadResult = await services!.upload.uploadAll();
      const per = r.devices.map((d) => `${d.deviceId.slice(-6)}: ${d.uploaded} up, secured→${d.serverAckedThrough}${d.error ? ` (${d.error})` : ''}`);
      return `${r.uploaded} uploaded, ${r.rejected} rejected, ${r.failed} failed\n${per.join('\n')}`;
    });

  return (
    <ScrollView style={styles.screen} contentContainerStyle={styles.content}>
      <Text style={styles.title}>Sensors</Text>
      {services?.usingFakeSensors && <Text style={styles.note}>Using fake sensors (Settings to change)</Text>}

      <View style={styles.row}>
        <Button label="Scan" onPress={scan} disabled={!services || !!busy} />
        <Button label="Collect all" onPress={collectAll} disabled={!services || !!busy || nearby.length === 0} />
        <Button label="Upload" onPress={upload} disabled={!services || !!busy} />
      </View>

      {busy && (
        <View style={styles.busy}>
          <ActivityIndicator />
          <Text style={styles.busyText}>{busy}…</Text>
        </View>
      )}
      {error && <Text style={styles.error}>{error}</Text>}
      {lastResult && <Text style={styles.result}>{lastResult}</Text>}

      <Text style={styles.section}>In range</Text>
      {nearby.length === 0 ? (
        <Text style={styles.empty}>Tap Scan. Sensors advertise their pending count, so an empty one shows 0.</Text>
      ) : (
        nearby.map((a) => (
          <Pressable key={a.id} style={styles.card} onPress={() => collect(a)} disabled={!!busy}>
            <View style={styles.cardHead}>
              <Text style={styles.cardTitle}>{a.name ?? a.id}</Text>
              <Text style={styles.dim}>{a.rssi != null ? `${a.rssi} dBm` : ''}</Text>
            </View>
            {a.advertising ? (
              <Text style={styles.cardLine}>
                pending {a.advertising.pending}
                {a.advertising.batteryMv ? ` · ${(a.advertising.batteryMv / 1000).toFixed(2)} V` : ''}
                {a.advertising.timeSet ? ' · clock set' : ' · clock unset'}
                {a.advertising.dropped ? ' · has dropped records' : ''}
              </Text>
            ) : (
              <Text style={styles.cardLine}>no telemetry advertisement</Text>
            )}
            <Text style={styles.hint}>tap to collect</Text>
          </Pressable>
        ))
      )}

      <Text style={styles.section}>Known sensors</Text>
      {devices.length === 0 ? (
        <Text style={styles.empty}>Nothing collected yet.</Text>
      ) : (
        devices.map((d) => {
          const secured = Math.max(d.serverAckedThrough, d.sensorSecuredThrough);
          const awaitingVisit = d.serverAckedThrough > d.sensorSecuredThrough;
          return (
            <View key={d.deviceId} style={styles.card}>
              <View style={styles.cardHead}>
                <Text style={styles.cardTitle}>TLM-{d.deviceId.slice(-6).toUpperCase()}</Text>
                <Text style={styles.dim}>{ago(d.lastSeenAt)}</Text>
              </View>
              <View style={styles.stats}>
                <Stat label="on sensor" value={d.pendingOnSensor ?? '?'} />
                <Stat label="collected" value={d.counts.collected} />
                <Stat label="uploaded" value={d.counts.uploaded} />
                <Stat label="secured →" value={secured} />
              </View>
              <Text style={styles.cardLine}>
                {d.deviceId} · fw {d.fwVersion ?? '?'} · boot {d.lastBootId ?? '?'} · next seq {d.sensorNextSeq ?? '?'}
              </Text>
              {awaitingVisit && (
                <Text style={styles.hint}>server confirmed through {d.serverAckedThrough}; the next visit frees the sensor</Text>
              )}
            </View>
          );
        })
      )}
    </ScrollView>
  );
}

function Button({ label, onPress, disabled }: { label: string; onPress: () => void; disabled?: boolean }) {
  return (
    <Pressable onPress={onPress} disabled={disabled} style={[styles.button, disabled && styles.buttonDisabled]}>
      <Text style={styles.buttonText}>{label}</Text>
    </Pressable>
  );
}

function Stat({ label, value }: { label: string; value: number | string }) {
  return (
    <View style={styles.stat}>
      <Text style={styles.statValue}>{value}</Text>
      <Text style={styles.statLabel}>{label}</Text>
    </View>
  );
}

const styles = StyleSheet.create({
  screen: { flex: 1, backgroundColor: '#f4f5f7' },
  content: { padding: 16, paddingTop: 56, paddingBottom: 40, gap: 10 },
  title: { fontSize: 26, fontWeight: '700', color: '#1b1f24' },
  note: { color: '#8a5a00', backgroundColor: '#fff4d6', padding: 8, borderRadius: 6 },
  row: { flexDirection: 'row', gap: 8, marginTop: 4 },
  button: { flex: 1, backgroundColor: '#1f6feb', paddingVertical: 12, borderRadius: 8, alignItems: 'center' },
  buttonDisabled: { opacity: 0.4 },
  buttonText: { color: '#fff', fontWeight: '600' },
  busy: { flexDirection: 'row', alignItems: 'center', gap: 8, paddingVertical: 4 },
  busyText: { color: '#57606a' },
  error: { color: '#b42318', backgroundColor: '#fee4e2', padding: 10, borderRadius: 6 },
  result: { color: '#1b1f24', backgroundColor: '#e6f4ea', padding: 10, borderRadius: 6, fontFamily: 'Menlo', fontSize: 12 },
  section: { marginTop: 14, fontSize: 13, fontWeight: '600', color: '#6e7781', textTransform: 'uppercase', letterSpacing: 0.6 },
  empty: { color: '#6e7781' },
  card: { backgroundColor: '#fff', borderRadius: 10, padding: 12, gap: 6, borderWidth: 1, borderColor: '#e6e8eb' },
  cardHead: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'baseline' },
  cardTitle: { fontSize: 16, fontWeight: '600', color: '#1b1f24' },
  cardLine: { color: '#57606a', fontSize: 13 },
  dim: { color: '#8b949e', fontSize: 12 },
  hint: { color: '#1f6feb', fontSize: 12 },
  stats: { flexDirection: 'row', justifyContent: 'space-between', marginVertical: 4 },
  stat: { alignItems: 'center', flex: 1 },
  statValue: { fontSize: 20, fontWeight: '700', color: '#1b1f24', fontVariant: ['tabular-nums'] },
  statLabel: { fontSize: 11, color: '#6e7781' },
});
