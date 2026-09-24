# ADR 0001: Record identity and a fixed binary record format

Date: 2026-09-23
Status: accepted

## Context

Readings were serialised ad hoc in six places (three firmware serialisers, a
Go struct, two TypeScript types) and had already drifted: the app expected
`moisture`, the firmware sent `resistance_ohms`. Readings had no identity
beyond a config string `node_id` shared by every board flashed from the
example config, and no sequence number, so nothing could be acknowledged,
resumed, or deduplicated.

The pipeline the system is being built for is store-and-forward across three
hops with an end-to-end acknowledgement. That requires every reading to be
nameable.

## Decision

1. Every reading is identified by `(device_id, seq)`. `device_id` derives from
   the chip's factory MAC. `seq` is a 32-bit monotonic counter persisted in
   NVS. A 16-bit `boot_id` persisted in NVS disambiguates uptime-based times.
2. Readings are stored, transferred over BLE, and carried inside upload
   envelopes as a **fixed 20-byte little-endian binary record** with a version
   byte, a type byte, a quality byte, flags, `seq`, a 32-bit time, a 32-bit
   float value, `boot_id`, and a CRC-16. The format is defined once in
   `schema/PROTOCOL.md` with golden vectors in `schema/vectors/`.
3. The record format is treated as a stable ABI. Fields are never reordered or
   reinterpreted. Changes add a new version byte.
4. The value is generic (`type` + `float32`). Battery voltage, temperature and
   future channels are new type ids, not new formats.

## Alternatives considered

- **JSON everywhere.** Simple and already present, but 5 to 10 times larger
  over BLE, awkward to store in a fixed-slot ring, and the drift already
  happened with it.
- **Protobuf via nanopb.** Good schema evolution and codegen for all three
  languages. Adds a toolchain, variable-length records complicate the ring
  store, and the phone-to-server hop does not need the bytes saved. Kept as an
  option for the HTTP envelope later.
- **CBOR.** Compact and self-describing but no schema enforcement; would still
  need vectors to stay honest. No advantage over a fixed struct at this size.

## Consequences

- The ring store can address slots by index and seek by `seq` in O(1).
- BLE chunks are N whole records plus a small header; no framing ambiguity.
- Each language has a 50-line codec and a vector test. Drift becomes a CI
  failure instead of a field bug.
- A firmware upgrade never invalidates stored data; the version byte is per
  record.
