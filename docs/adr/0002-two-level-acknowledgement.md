# ADR 0002: Two-level acknowledgement and explicit full-buffer policy

Date: 2026-09-23
Status: accepted

## Context

The requirement is that a sensor frees a reading's storage only once there is
round-trip confirmation that the reading is written to the server database.
But the phone collects readings in the field, where it typically has no
internet. The server's confirmation can only reach the sensor on the phone's
next visit. Meanwhile the person standing at the hole wants to see that their
data is safe.

## Decision

1. Two acknowledgement levels are tracked per record, on the phone and in the
   UI: **collected** (stored in the phone's SQLite) and **secured** (the server
   returned the record's `seq` inside an acked range).
2. The phone acks *collected* to the sensor immediately during the visit. The
   sensor records it but does **not** free the slot.
3. The phone acks *secured* to the sensor on the first visit after the server
   confirmed. The sensor frees slots through that `seq`.
4. When the ring is full, behaviour is explicit configuration:
   `drop_oldest_unacked` (default, increments a dropped counter reported in
   device info) or `stop_sampling`. A third option, `free_on_collected_when_
   above_percent`, lets an operator trade the strict server round trip for
   capacity on sensors that are visited rarely.
5. The UI always shows both counts. "Secured" is never shown as a proxy for
   "collected".

## Alternatives considered

- **Free on phone ack only.** Simpler, but a phone lost or wiped before upload
  loses data with no trace, which contradicts the requirement.
- **Free on server ack only, no policy.** Meets the letter of the requirement
  but a sensor visited less often than its ring capacity silently stops
  recording or silently overwrites, with no operator choice.

## Consequences

- Sensor ring capacity must be sized to interval × visit period × safety
  factor. At 60 s and 20 bytes a megabyte holds about 36 days.
- The sensor keeps two cursors: `collected_through` and `secured_through`.
  Only the second moves the ring's tail.
- The app needs a durable per-device "pending secured ack" list that survives
  restarts.
