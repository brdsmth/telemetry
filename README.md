# Telemetry

Buried ESP32 soil sensors, a mobile app that collects their readings over
Bluetooth, and a server that keeps them. The pipeline is store-and-forward with
an end-to-end acknowledgement: a sensor frees a reading only after the server
has confirmed it.

Start with [`docs/ARCHITECTURE.md`](./docs/ARCHITECTURE.md). Testing strategy
is in [`docs/TESTING.md`](./docs/TESTING.md). Decisions are recorded in
[`docs/adr/`](./docs/adr/). Wire formats are in
[`schema/PROTOCOL.md`](./schema/PROTOCOL.md).

## Layout

| Directory   | What                                                        |
|-------------|-------------------------------------------------------------|
| `schema/`   | Wire formats, golden test vectors, protocol document        |
| `lib/`      | Shared firmware core, buildable on the host                 |
| `sensor/`   | ESP32 soil sensor firmware                                  |
| `gateway/`  | ESP32-S3 + SIM7670G cellular gateway firmware (parked)      |
| `app/`      | Expo / React Native mobile app                              |
| `api/`      | Go HTTP API backed by Postgres                              |
| `infra/`    | Deployment                                                  |
| `docs/`     | Architecture, testing, ADRs                                 |

## Working on it

```bash
make test        # host tests for every component
make -C sensor build   # compile the sensor firmware
docker compose up -d   # local Postgres for the api
```

Commit conventions are in [`AGENTS.md`](./AGENTS.md).
