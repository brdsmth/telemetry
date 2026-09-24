# ADR 0004: A fixed-slot ring in flash is the sensor's record store

Date: 2026-09-24
Status: accepted

## Context

The sensor must keep every reading until a phone has collected it and the
server has confirmed it, which can be days. It has a few megabytes of SPI
flash and a small NVS partition. Flash has two properties that shape any
storage scheme built on it:

- It cannot be overwritten in place. Changing a byte means erasing a whole
  block, typically 4 KB, and writing it again.
- Erasing is slow and wears the cells out after roughly 100k cycles per
  block, so writes should land on fresh space and be spread evenly.

The readings themselves are fixed size (20 bytes, ADR 0001), arrive at a fixed
rate, and are consumed oldest first. The only operations the protocol needs
are "append a reading" and "give me the readings with `seq >= N`, in order",
plus an acknowledgement that frees everything up to a `seq`.

## Decision

Readings are stored in a **ring of fixed-size slots**: a fixed number of
20-byte slots where a reading with sequence number `seq` always lives in slot
`seq % capacity`. Writes advance around the ring and wrap; nothing is ever
moved or compacted. A small cursor (`next_seq`, `tail_seq`,
`collected_through`, `secured_through`, `dropped`) is persisted separately in
NVS as one blob and defines which slots hold live data. Only `secured_through`
frees slots; `collected_through` is bookkeeping for the phone (ADR 0002).

The ring is implemented today as **one fixed-size file on LittleFS**, behind a
two-method slot storage interface (`readSlot`, `writeSlot`). LittleFS provides
wear levelling and power-loss safety at the file level. A raw flash partition
with our own erase-block management can replace it behind the same interface
if LittleFS becomes a bottleneck or a corruption source.

## Why this shape

- **Flash-friendly by construction.** Every slot is written equally often, so
  wear is spread without a wear-levelling layer of our own, and no data is
  rewritten to make room.
- **O(1) everything.** Seek by `seq` is arithmetic. Append is one write. Ack
  is a cursor update. No index, no scan, no compaction.
- **Bounded and honest.** Capacity is explicit (`ring_slots` in config, 8192
  by default, about 5.7 days at one reading a minute). When it is full, the
  policy is explicit configuration: drop the oldest unsecured reading and
  count it, or stop sampling.
- **Matches the access pattern.** The phone always wants a contiguous range
  starting at a `seq`, which is a contiguous run of slots.
- **Standard.** The kernel log buffer, network card descriptor rings and the
  internals of flash filesystems are the same structure. It is the usual
  answer for a durable fixed-rate queue on flash.

## Alternatives considered

- **Append-only CSV or binary log** (what the firmware had). Simple, but grows
  without bound, freeing old data means rewriting the file, and finding
  reading N is a scan. Also what caused the data loss on header change.
- **One file per reading or per day.** Filesystem metadata overhead and many
  small files, which flash filesystems handle poorly.
- **NVS key-value store.** Designed for a handful of settings; tens of
  thousands of records would wear out its small partition.
- **Embedded database.** More flexibility than the problem has questions, and
  it would still need a filesystem and an index underneath.
- **Raw partition ring.** Same pattern one layer lower, faster and more
  robust, more code (erase-block headers, boot-time recovery scan). Deferred,
  not rejected; the interface leaves the door open.

## Consequences

- Capacity changes remap `seq -> slot`, so the cursor blob records the
  capacity and a mismatch resets the ring. Resize is a deliberate wipe.
- A corrupt slot is skipped on read and counted; the phone sees the gap in
  `seq` and the sensor never fabricates a filler.
- The ring is host-testable: the same code runs over an in-memory slot array
  in the Unity tests and the simulator.
- Related: [ADR 0001](./0001-record-identity-and-binary-format.md) for why
  records are fixed size, [ADR 0002](./0002-two-level-acknowledgement.md) for
  what frees a slot.
