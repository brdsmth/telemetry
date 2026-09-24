# ADR 0003: The Go API with Postgres is the system of record

Date: 2026-09-23
Status: accepted

## Context

Two backends exist. `api/` is a Go HTTP server writing to Postgres, used by the
sensor over WiFi. `infra/` deploys AWS API Gateway → EventBridge → SQS → a
Lambda whose handler is a TODO that logs and discards; the cellular gateway
posts there with a different payload shape. Neither is complete and they share
no schema.

## Decision

The Go API backed by Postgres is the single system of record. It gains a batch
endpoint with idempotency on `(device_id, seq)`, sessions and batches tables,
a connection pool, and migrations. The gateway, when it returns, posts the same
batch envelope to the same endpoint.

The Go API and its Postgres run on Railway, in the `telemetry` project, with
the API built from `api/Dockerfile` and configured by `api/railway.json`. The
AWS event pipeline is not extended and `infra/` is slated for removal once
nothing references it. If a managed ingress is wanted later it sits in front
of the Go API rather than beside it.

## Alternatives considered

- **Finish the AWS path.** Serverless ingest is attractive for cellular
  bursts, but the Lambda would still need Postgres, the schema would still
  need to be shared, and two write paths double every test.
- **MQTT broker.** Listed in the old infra README. Sensible for always-on
  gateways, irrelevant for the BLE-to-phone path, and premature.

## Consequences

- One schema, one set of integration tests, one place to add tracing.
- `infra/index.ts` with its hardcoded account and resource ids is slated for
  removal.
- Deployment configuration lives with the service (`api/railway.json`) rather
  than in a separate infrastructure tree.
- The gateway's `Telemetry` struct is replaced by the shared record format
  when that work resumes.
