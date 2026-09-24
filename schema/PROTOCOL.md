# Wire formats

This directory is the stable ABI of the system. Every byte layout that crosses
a component boundary is defined here and pinned by golden vectors in
[`vectors/`](./vectors/). Firmware, app and server each have a codec that is
tested against those vectors. See [`docs/TESTING.md`](../docs/TESTING.md).

Rules:

- Formats are versioned. A change that alters the meaning of existing bytes
  gets a new version number. Fields are never reordered.
- Add the vector first, then make the codecs pass.
- All multi-byte integers are **little-endian**. Floats are IEEE 754 binary32.
- `PROTOCOL_VERSION` is currently **1**. It is independent of firmware and app
  versions and is reported in the BLE device info so the app can refuse or
  adapt.

## 1. Record (version 1)

One reading. The same 20 bytes live in the sensor's flash ring, travel over
BLE, and are carried base64-encoded inside the upload envelope.

| Offset | Size | Type    | Field     | Meaning                                                    |
|-------:|-----:|---------|-----------|------------------------------------------------------------|
|      0 |    1 | u8      | `version` | Always `1`.                                                |
|      1 |    1 | u8      | `type`    | Reading type id, see §1.1.                                 |
|      2 |    1 | u8      | `quality` | See §1.2.                                                  |
|      3 |    1 | u8      | `flags`   | Bit field, see §1.3.                                       |
|      4 |    4 | u32     | `seq`     | Monotonic per device, starts at 1. Persisted across boots. |
|      8 |    4 | u32     | `time`    | Unix seconds if `EPOCH_VALID`, else seconds since boot.    |
|     12 |    4 | f32     | `value`   | The reading, in the unit given by `type`.                  |
|     16 |    2 | u16     | `boot_id` | Incremented on every cold boot. Persisted.                 |
|     18 |    2 | u16     | `crc`     | CRC-16/CCITT-FALSE over bytes 0..17.                       |

Total size: **20 bytes**.

A fresh device assigns `seq = 1` to its first record. `0` is never assigned and
means "none" in every cursor (`collected_through`, `secured_through`). The
codec itself accepts `seq = 0`; only the store refuses to issue it.

CRC-16/CCITT-FALSE: polynomial `0x1021`, initial value `0xFFFF`, no input or
output reflection, no final XOR. Check value for the ASCII string
`123456789` is `0x29B1`.

Decoders must reject, in this order of checking:

| Condition                      | Error                 |
|--------------------------------|-----------------------|
| fewer than 20 bytes            | `short_input`         |
| `version` is not 1             | `unsupported_version` |
| CRC mismatch                   | `bad_crc`             |

Decoders must **not** reject unknown `type` values; new reading types are
added without a format change. `quality` is decoded as a raw byte.

### 1.1 Reading types

| `type` | Name                    | Unit          | Notes                                       |
|-------:|-------------------------|---------------|---------------------------------------------|
|      1 | `soil_resistance_ohms`  | ohm           | Gypsum block resistance. `-1` when open.    |
|      2 | `battery_millivolts`    | mV            | Supply or cell voltage as seen by the board.|
|      3 | `board_temperature_c`   | °C            | Reserved.                                   |

Ids 200–255 are reserved for experiments and never appear in vectors.

### 1.2 Quality

| `quality` | Name    | Meaning                                                |
|----------:|---------|--------------------------------------------------------|
|         0 | `ok`    | Within the ADC's characterised range.                  |
|         1 | `low`   | Below the usable floor: very wet or shorted.           |
|         2 | `high`  | Above the usable ceiling: very dry, rough estimate.    |
|         3 | `open`  | No path to ground: sensor disconnected.                |

### 1.3 Flags

| Bit | Name          | Meaning                                                   |
|----:|---------------|-----------------------------------------------------------|
|   0 | `EPOCH_VALID` | `time` is unix seconds. Clear: `time` is seconds since boot. |
| 1–7 | reserved      | Must be zero in version 1.                                |

### 1.4 Time reconstruction

A record without `EPOCH_VALID` can be given a wall-clock time by the phone if
it was produced during the boot the phone connected in:

```
epoch = phone_time_at_connect - sensor_uptime_at_connect + record.time
        (only when record.boot_id == boot_id_at_connect)
```

Otherwise the wall-clock time is unknown. The raw `time`, `boot_id` and
`flags` are always preserved so the reconstruction is auditable. The server
stores a `time_source` of `sensor`, `phone_backfill` or `unknown`.

## 2. Device identity

`device_id` is the 6-byte factory base MAC of the ESP32, rendered as 12
lowercase hex characters with no separators (`aabbccddeeff`) wherever it is
text, and as 6 raw bytes wherever it is binary. It is never read from config.

The BLE device name is `TLM-` followed by the last three MAC bytes in
uppercase hex (`TLM-DDEEFF`). The full id is in the device info
characteristic.

## 3. BLE sync service

Specified here so all sides build against one description. The characteristic
payloads below get vectors when their codecs land; until then this section is
the design of record.

Service UUID: `6eaedc54-f770-40e3-9806-a6ccf63c8099`

Characteristics replace the third group with the values below.

| Characteristic | UUID                                   | Properties     | Payload            |
|----------------|----------------------------------------|----------------|--------------------|
| Device Info    | `6eaedc54-f770-40e3-0001-a6ccf63c8099` | READ           | §3.2               |
| Control        | `6eaedc54-f770-40e3-0002-a6ccf63c8099` | WRITE (w/ rsp) | one command, §3.3  |
| Data           | `6eaedc54-f770-40e3-0003-a6ccf63c8099` | NOTIFY         | one chunk, §3.4    |
| Status         | `6eaedc54-f770-40e3-0004-a6ccf63c8099` | READ, NOTIFY   | §3.5               |

The sensor requests an MTU of 512. The phone should request the same.

### 3.1 Advertising

The advertisement carries flags, the 128-bit service UUID, and a manufacturer
specific data field with company id `0xFFFF` (test id) and this 6-byte payload:

| Offset | Size | Field              | Meaning                                              |
|-------:|-----:|--------------------|------------------------------------------------------|
|      0 |    1 | `protocol_version` | `1`                                                  |
|      1 |    2 | `pending`          | Records not yet collected, saturating at 65535.      |
|      3 |    2 | `battery_mv`       | `0` if unknown.                                      |
|      5 |    1 | `adv_flags`        | bit 0: time is set. bit 1: ring has dropped records. |

The device name is in the scan response. A phone can list sensors and skip
ones with `pending == 0` without connecting.

### 3.2 Device Info (read)

| Offset | Size | Field               |
|-------:|-----:|---------------------|
|      0 |    1 | `protocol_version`  |
|      1 |    1 | `record_size` (20)  |
|      2 |    6 | `device_id`         |
|      8 |    2 | `boot_id`           |
|     10 |    4 | `next_seq`          |
|     14 |    4 | `collected_through` |
|     18 |    4 | `secured_through`   |
|     22 |    4 | `uptime_s`          |
|     26 |    4 | `unix_time` (0 if unset) |
|     30 |    4 | `dropped`           |
|     34 |    2 | `battery_mv`        |
|     36 |    2 | `capacity` (slots)  |
|     38 |   16 | `fw_version`, NUL padded ASCII |

Total 54 bytes.

### 3.3 Control commands (write)

First byte is the opcode. Every command is answered by a Status notification.
Payloads are exact length: a command of any other length, or with an unknown
opcode, is answered with `bad_argument` and the first byte echoed as `opcode`.
Every command except `OPEN_SESSION` needs an open session; otherwise the
answer is `no_session`.

| Opcode | Name             | Payload                          | Effect                                              |
|-------:|------------------|----------------------------------|-----------------------------------------------------|
| `0x01` | `OPEN_SESSION`   | `session_id` 16 bytes            | Starts a session; resets chunk state.               |
| `0x02` | `SET_TIME`       | `unix_time` u32                  | Sets the RTC and persists the offset. `0` is `bad_argument`. |
| `0x03` | `READ_FROM`      | `seq` u32, `max_records` u16     | Streams records with `seq >= from` as Data chunks. `max_records = 0` means all. Replaces a stream in progress. A `seq` below the oldest stored record is clamped; Status `seq` reports the start. |
| `0x04` | `ACK_COLLECTED`  | `through_seq` u32                | Advances `collected_through`. Frees nothing.        |
| `0x05` | `ACK_SECURED`    | `through_seq` u32                | Advances `secured_through`; frees slots.            |
| `0x06` | `CLOSE_SESSION`  | none                             | Ends the session; sensor may return to sleep.       |

### 3.4 Data chunk (notify)

| Offset | Size | Field       | Meaning                                           |
|-------:|-----:|-------------|---------------------------------------------------|
|      0 |    4 | `first_seq` | `seq` of the first record in this chunk.          |
|      4 |    1 | `count`     | Number of records, 0..24. 0 only with `last` set.  |
|      5 |    1 | `flags`     | bit 0: last chunk for this `READ_FROM`.           |
|      6 |    2 | `crc`       | CRC-16/CCITT-FALSE over the record bytes.         |
|      8 | 20·n | records     | `count` consecutive records.                      |

Records within a chunk have consecutive `seq` values. Gaps are the phone's
signal that records were dropped; the sensor never fabricates fillers. A chunk
with `count = 0` carries `last` and means nothing is stored at or after
`first_seq`. Records are sent in order until a chunk with `last` arrives.

### 3.5 Status (read / notify)

| Offset | Size | Field      | Meaning                                       |
|-------:|-----:|------------|-----------------------------------------------|
|      0 |    1 | `opcode`   | Command being answered, `0` if none.          |
|      1 |    1 | `result`   | See below.                                    |
|      2 |    4 | `seq`      | Cursor relevant to the result, e.g. new `collected_through`. |

| `result` | Meaning              |
|---------:|----------------------|
|        0 | `ok`                 |
|        1 | `no_session`         |
|        2 | `bad_argument`       |
|        3 | `seq_out_of_range`   |
|        4 | `busy`               |

## 4. Upload envelope

`POST /v1/batches`, JSON. Records are the raw 20 bytes, base64 encoded, so the
server decodes them with the same codec as everyone else.

```json
{
  "batch_id": "0d4a8a58-7c8f-4a49-9b4a-3b7e1f2d9c11",
  "session_id": "7a1c2e3f-1234-4bcd-9ef0-0123456789ab",
  "phone_id": "c0ffee00-aaaa-4bbb-8ccc-dddddddddddd",
  "device_id": "aabbccddeeff",
  "protocol_version": 1,
  "phone_time_at_connect": 1790121600,
  "sensor_uptime_at_connect": 3600,
  "sensor_boot_id_at_connect": 3,
  "records": ["AQEAAQEAAAAA...", "..."]
}
```

Response `200`:

```json
{
  "batch_id": "0d4a8a58-7c8f-4a49-9b4a-3b7e1f2d9c11",
  "acked": [{"from_seq": 1, "to_seq": 40}, {"from_seq": 42, "to_seq": 60}],
  "rejected": [{"index": 12, "reason": "bad_crc"}]
}
```

A record whose `(device_id, seq)` already exists is acknowledged, not
rejected. `acked` ranges cover every record the server holds after this
request, so the phone can advance `secured_through` to the highest contiguous
`to_seq` from the sensor's `secured_through`.

`4xx` means the envelope itself is malformed and nothing was stored. `5xx`
means retry the whole batch; idempotency makes that safe.
