# Architecture

This document describes the target architecture of the telemetry system and the
rules that keep it maintainable as it grows. It is a living document: when a
decision here changes, update it and record why in a new ADR under
[`docs/adr/`](./adr/).

For the wire formats see [`schema/PROTOCOL.md`](../schema/PROTOCOL.md). For how
each layer is tested see [`TESTING.md`](./TESTING.md).

## 1. What the system does

Battery powered ESP32 sensors are buried in the ground. Each one samples on a
configurable interval, stores every reading locally, and advertises over
Bluetooth Low Energy in a way that conserves battery. A person with the mobile
app walks past, the app detects the sensors, connects, pulls every reading the
phone has not seen, and stores them on the phone. When the phone has internet
it uploads the readings to the server. Once the server confirms the readings
are durable, the phone tells the sensor on its next visit, and the sensor frees
that storage.

This is a **store-and-forward pipeline with end-to-end acknowledgement across
three hops**:

```
 sensor flash  ──BLE──▶  phone SQLite  ──HTTPS──▶  server Postgres
      ▲                       │                          │
      └──── ack (next visit) ─┴──────── ack (response) ──┘
```

Cellular connectivity through the gateway is a later option that will reuse the
same record format and the same server API. The mobile UI stays basic until the
pipeline is proven.

## 2. The one idea everything hangs on: record identity

Every reading is identified by `(device_id, seq)`.

| Field       | Source                                                           |
|-------------|------------------------------------------------------------------|
| `device_id` | Derived from the chip's factory MAC. Never from config.          |
| `seq`       | 32-bit counter, monotonic per device, persisted in NVS.          |
| `boot_id`   | 16-bit counter incremented on each cold boot, persisted in NVS.  |

With a stable identity:

- the phone can ask a sensor for "everything after `seq` N",
- the server can insert idempotently and ignore duplicates,
- an acknowledgement can name exactly which records are durable,
- a record can be traced through all three stores.

The identity lives in the binary record format defined in
[`schema/PROTOCOL.md`](../schema/PROTOCOL.md) and is the same bytes in flash,
over BLE, and inside the upload envelope. The record format is versioned and
treated like a stable ABI: fields are never reordered or reinterpreted, only
new versions are added.

## 3. Components

### 3.1 Sensor firmware (`sensor/`, shared core in `lib/`)

The firmware is split into three layers. Code may depend downward only.

```
sensor/src/app/          wiring: reads config, constructs platform objects,
                         hands them to the core, runs the scheduler
lib/telemetry-core/      pure C++17, no Arduino, no ESP-IDF: record codec,
                         CRC, ring store, sync protocol, scheduler
sensor/src/hal/          abstract interfaces the core depends on
sensor/src/platform/     ESP32 implementations of the HAL (ADC, LittleFS,
                         NVS, NimBLE, RTC, deep sleep)
```

Rules:

- Anything that does not touch a peripheral lives in `lib/telemetry-core` and
  compiles on the host. This is what makes it testable.
- The core never includes `Arduino.h` and never uses Arduino `String`. It uses
  fixed buffers and `std::` types.
- Interfaces are small abstract classes and live next to the code that needs
  them. The core defines the ones it depends on (slot storage, cursor store,
  system services) inside `lib/telemetry-core`; `sensor/src/hal/` holds the
  peripheral-facing ones (probe, power). Add an interface only when a second
  implementation exists or a test needs one.
- Build variants are PlatformIO environments, not runtime flags. `field` has
  BLE, the store, and deep sleep. `bench` adds WiFi, NTP, the web portal, and
  verbose serial. Runtime flags do not shrink the binary and make radio
  behaviour untestable.

Subsystems inside the core:

- **Record codec.** Encode and decode the fixed 20-byte record with CRC.
- **Ring store.** Fixed-size slots on a block device. Operations: append,
  iterate from a `seq`, acknowledge through a `seq`, report free slots and
  dropped count. A cursor (`head`, `tail`, `next_seq`) is persisted in NVS.
  The full-buffer policy is explicit configuration: drop the oldest unacked
  record or stop sampling.
- **Sync session.** A state machine that turns commands from the phone into
  chunks of records and applies acknowledgements. It has no BLE dependency.
  A thin transport adapter feeds it bytes.
- **Scheduler.** A state machine over `Sleeping → Sampling → Advertising →
  Connected → Sleeping`. The wake and advertise cadence is a `PowerPolicy`
  value, not hard-coded delays.

Time: a buried sensor has no clock source. Every record carries either a unix
time or seconds since boot plus `boot_id`, distinguished by a flag. The phone
writes the current time as its first command on connect. The sensor persists
the offset so later records and later wakes from deep sleep carry real time.
The phone back-fills time for earlier records from the same boot using the
uptime it observed at connect, and the server stores the time source with
each record.

Power: the ESP32 cannot advertise from deep sleep, so "connect instantly" and
"last for months" are in tension. The baseline is a duty cycle of deep sleep
with wake windows that advertise for a few seconds. Advertising payload
carries the pending record count and battery so the phone can list sensors
and skip empty ones without connecting. Light sleep with BLE modem sleep is an
optimisation to measure, not assume; Arduino's prebuilt SDK config may limit
it and the fallback is the ESP-IDF framework.

### 3.2 Mobile app (`app/`)

Layered so the UI is thin and the sync logic is testable without a device.

```
app/src/protocol/   record and command codec (pure TypeScript, vector tested)
app/src/transport/  SensorTransport interface: BlePlxTransport, FakeTransport,
                    TcpTransport (dev only, talks to the simulator)
app/src/store/      SQLite repositories: sensors, readings, sessions, batches
app/src/sync/       CollectionService (BLE side), UploadService (HTTP side)
app/src/net/        API client, connectivity watcher
app/app/            expo-router screens that render store state only
```

Collection: scan filtered by the telemetry service UUID, connect, set time,
read device info, pull from the phone's cursor for that device, store, ack
"received by phone", and queue any pending "durable on server" acks.

Upload: batch unsent readings per device, POST, mark each acked range as
uploaded, and schedule the sensor ack for the next visit. Triggered on
connectivity change and app foreground; background tasks are a later step.

Two acknowledgement levels are always visible in the UI: **collected** (on the
phone) and **secured** (on the server). In the field with no internet, the
only honest confirmation is "collected".

### 3.3 Server API (`api/`)

Go with Postgres. The system of record.

```
api/cmd/api/           main: config, wiring, HTTP server
api/internal/schema/   record decoder, shared types, vector tests
api/internal/ingest/   HTTP handlers: POST /v1/batches, GET /healthz
api/internal/store/    Postgres access, migrations
```

`POST /v1/batches` takes a device id, phone id, session id, and a list of
records, inserts with a unique constraint on `(device_id, seq)` ignoring
duplicates, and returns the acknowledged sequence ranges. Idempotency lets the
phone retry safely. Sessions and batches are stored as tables, which is what
makes tracing a query rather than a log search.

The AWS API Gateway → EventBridge → SQS → Lambda path under `infra/` is a
leftover from the cellular gateway experiments and discards data. It is not
the backend. See [ADR 0003](./adr/0003-go-api-is-system-of-record.md).

### 3.4 Gateway (`gateway/`)

Cellular uplink on an ESP32-S3 with a SIM7670G. Not on the current critical
path. When it returns it will consume the same record format from
`lib/telemetry-core` and post the same batch envelope to the Go API.

## 4. Sessions and tracing

Identifiers, all carried in the wire formats:

| Id           | Minted by | Scope                          |
|--------------|-----------|--------------------------------|
| `device_id`  | sensor    | forever                        |
| `boot_id`    | sensor    | one power cycle                |
| `seq`        | sensor    | one record                     |
| `session_id` | phone     | one BLE connection             |
| `batch_id`   | phone     | one upload request             |
| `phone_id`   | phone     | one app install                |

Every hop logs the session, device, sequence range and outcome. The server
stores sessions and batches. "Which visit collected reading 4711 and when did
it land" is a query. OpenTelemetry on the server is a later addition; the
correlation ids must exist now.

## 5. Repository layout (target)

```
schema/         wire formats, golden vectors, protocol doc  (the stable ABI)
lib/            shared firmware core, host-buildable
sensor/         sensor firmware: app wiring, HAL, ESP32 platform, tests
gateway/        cellular gateway firmware
app/            Expo mobile app
api/            Go server
infra/          deployment (to be reduced to what the Go API needs)
docs/           this document, TESTING.md, ADRs
scripts/        developer tooling: serial capture, BLE probe, HIL smoke
.github/        CI
Makefile        root task runner: make test fans out to every component
```

## 6. Conventions

- Commits follow [AGENTS.md](../AGENTS.md). Scopes map to the directories
  above; `schema` is a scope.
- Every commit builds and passes host tests. CI enforces this.
- Decisions with lasting consequences get an ADR. Small ones get a commit body.
- Firmware and protocol are versioned separately. The app reads the protocol
  version from the device and refuses or adapts.

## 7. Roadmap

1. **Foundation.** `schema/` with the record format and vectors, root task
   runner, CI, `lib/telemetry-core` with the codec and ring store under host
   tests. *(in progress)*
2. **Sensor.** Ring store on LittleFS with NVS cursor, sync session over
   NimBLE, set-time command, `field` build with deep sleep, simulator build.
3. **App.** Transport interface with fake and BLE implementations, SQLite
   store, collection service, one screen showing pending / collected / secured
   per sensor.
4. **Server and upload.** Batch endpoint with idempotency, connection pool,
   migrations, upload service with connectivity trigger, server ack flowing
   back to the sensor on the next visit.
5. **Assurance.** End-to-end scenario against the simulator, hardware-in-the-
   loop smoke script, power measurements and tuning.

## 8. Known state of the current code

Recorded so the roadmap is honest about what is being replaced.

- The reading shape is defined in six places and has already drifted: the app
  expects `moisture`, the firmware sends `resistance_ohms`.
- The sensor's BLE characteristic carries only the latest reading as JSON.
- The CSV log is deleted on boot if its header changes.
- Without WiFi the sensor stamps records with seconds since boot.
- The API shared a single non-concurrent connection across handlers (fixed in
  the foundation work).
- The app's scan timeout captured a stale flag and never stopped the scan
  (fixed in the foundation work).
- `lib/` held duplicated WiFi and POST helpers; it is being repurposed as the
  shared core.
