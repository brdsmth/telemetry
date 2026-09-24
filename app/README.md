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

Then `npx expo start` reloads JavaScript into that build. On a simulator or on
web there is no Bluetooth; turn on "Use fake sensors" in Settings to drive
the whole pipeline against two in-memory sensors, including uploads to the
real API.

The API URL defaults to the Railway deployment and can be changed in
Settings.

## Tests

```bash
npm test          # Jest: codec vectors, fake sensor, collection and upload
npm run typecheck # tsc --noEmit
npx expo-doctor   # dependency and config health
```

`npm run lint` is currently broken by an eslint import resolver problem that
predates this code; CI runs the type check and the tests.

## Notes

- Bluetooth permissions are declared in `app.json`; iOS asks on first scan.
- A visit is one BLE connection: open session, `SET_TIME`, `ACK_SECURED` if
  the server confirmed anything since last time, `READ_FROM` windows until
  the sensor has nothing more, `ACK_COLLECTED`, close.
- Uploads are one batch per session so the server can back-fill times for
  records the sensor stamped with uptime.
