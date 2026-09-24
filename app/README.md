# Mobile app

Expo / React Native app that carries readings from sensors to the server:
scan for sensors over BLE, pull what they hold, store it on the phone,
upload when online, and pass the server's confirmation back to the sensor on
the next visit. See [`docs/ARCHITECTURE.md`](../docs/ARCHITECTURE.md) §3.2.

## Layout

```
src/protocol/    record, advertising, command, status and chunk codecs
                 (tested against ../schema/vectors)
src/transport/   SensorTransport interface; BlePlxTransport (real radio),
                 FakeSensor + FakeTransport (in-memory sensor for tests and dev)
src/store/       Repository interface; SqliteRepository, MemoryRepository
src/net/         ApiClient for POST /v1/batches; FetchApiClient, FakeApiClient
src/sync/        CollectionService (one BLE visit), UploadService (batches)
src/platform/    wiring: getServices() builds the real stack once
app/(tabs)/      two screens: Sensors, Settings
```

Screens only call `getServices()`. Nothing in `src/` imports React.

## Running

BLE is a native module, so **Expo Go cannot run this app**. Use a development
build:

```bash
npm install
npx expo run:ios        # or: npx expo run:android
```

Then `npx expo start` reloads JavaScript into that build. Simulators and web
have no Bluetooth and default to two in-memory fake sensors that speak the
protocol, so the whole pipeline, including uploads to the real API, can be
exercised without hardware; Settings can switch either way.

SDK 57 needs **Xcode 26.4 or newer** for a local iOS build; Xcode 26.3 fails
inside `expo-modules-jsi` (expo/expo#50067). Web needs no Xcode:

```bash
npx expo start --web --port 8081     # then open http://localhost:8081
```

Install `watchman` (`brew install watchman`); without it Metro's file
watcher misses edits and serves stale bundles until restarted with `--clear`.

The API URL defaults to the Railway deployment and can be changed in
Settings.

## Tests

```bash
npm test          # Jest: codec vectors, fake sensor, api client, collection and upload
npm run typecheck # tsc --noEmit
npx expo-doctor   # dependency and config health
maestro test maestro/pipeline.yaml   # UI smoke on a running simulator build
```

The Maestro flow scans, collects, uploads and revisits the fake sensors on a
development build. The same sequence was verified on the web target by
driving the page in a headless browser against the Railway API.

`npm run lint` is currently broken by an eslint import resolver problem that
predates this code; CI runs the type check and the tests.

## Notes

- Bluetooth permissions are declared in `app.json`; iOS asks on first scan.
- A visit is one BLE connection: open session, `SET_TIME`, `ACK_SECURED` if
  the server confirmed anything since last time, `READ_FROM` windows until
  the sensor has nothing more, `ACK_COLLECTED`, close.
- Uploads are one batch per session so the server can back-fill times for
  records the sensor stamped with uptime.
