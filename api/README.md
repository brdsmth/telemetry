# API

Go HTTP server backed by Postgres. The system of record for sensor readings
(see [ADR 0003](../docs/adr/0003-go-api-is-system-of-record.md)). Deployed on
Railway from `Dockerfile` with the service settings in `railway.json`.

## Endpoints

| Method | Path           | Purpose                                                              |
|--------|----------------|----------------------------------------------------------------------|
| POST   | `/v1/batches`  | Upload envelope from a phone, [`schema/PROTOCOL.md` §4](../schema/PROTOCOL.md). Idempotent on `(device_id, seq)`; answers with acked seq ranges. |
| POST   | `/ingest`      | Legacy one-reading JSON from the bench firmware. Answers `202 ok`.   |
| GET    | `/healthz`     | `200 ok` when Postgres answers, else `503`.                          |

## Layout

```
cmd/api/          main: config, wiring, HTTP server
internal/schema/  record codec, tested against schema/vectors
internal/ingest/  handlers: batches, legacy, healthz
internal/store/   Store interface, Postgres implementation, in-memory store for tests
```

Tables (created on start if missing): `readings` keyed by `(device_id, seq)`,
`sessions`, `batches`, and the legacy `incoming_raw`.

## Running locally

```bash
docker compose up -d                       # Postgres from the repo root
export DATABASE_URL=postgres://telemetry:changeme@localhost:5432/telemetry?sslmode=disable
make run                                   # listens on :8080, or $PORT
```

## Tests

```bash
make test        # handler tests against the in-memory store, codec vectors
DATABASE_URL=... make test   # also runs the Postgres store tests
```

The Postgres tests skip when `DATABASE_URL` is unset so a laptop without
Docker stays green. CI provides a Postgres service.

## Deploying

```bash
railway up ./api --path-as-root --service api --environment production --detach
railway deployment list --service api --json   # wait for SUCCESS
curl https://api-production-2e52.up.railway.app/healthz
```

`DATABASE_URL` on the service is the reference `${{Postgres.DATABASE_URL}}`.
