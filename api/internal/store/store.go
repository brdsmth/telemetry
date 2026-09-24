// Package store is the API's persistence boundary. Postgres implements it
// for real; MemStore implements it for handler tests.
package store

import (
	"context"
	"time"
)

// Reading is one decoded record plus what the server derived about it.
type Reading struct {
	DeviceID   string
	Seq        uint32
	Type       uint8
	Quality    uint8
	Flags      uint8
	RawTime    uint32 // as carried in the record: unix seconds or seconds since boot
	BootID     uint16
	Value      float32
	RecordedAt *time.Time // nil when the wall-clock time is unknown
	TimeSource string     // "sensor", "phone_backfill" or "unknown"
	BatchID    string
}

// Session is one BLE connection between a phone and a device.
type Session struct {
	SessionID             string
	DeviceID              string
	PhoneID               string
	PhoneTimeAtConnect    *time.Time
	SensorUptimeAtConnect *uint32
	SensorBootIDAtConnect *uint16
}

// Batch is one upload request, kept for tracing.
type Batch struct {
	BatchID       string
	SessionID     string
	DeviceID      string
	RecordCount   int
	InsertedCount int
	RejectedCount int
}

// LegacyReading is the pre-protocol JSON the bench firmware still posts.
type LegacyReading struct {
	Node      string
	Type      string
	Depth     int
	Firmware  string
	Value     float64
	Timestamp time.Time
}

// Row types for the admin read endpoints. JSON tags live here so the admin
// package can serve them directly.

type DeviceSummary struct {
	DeviceID       string     `json:"device_id"`
	ReadingCount   int64      `json:"reading_count"`
	MinSeq         uint32     `json:"min_seq"`
	MaxSeq         uint32     `json:"max_seq"`
	LastRecordedAt *time.Time `json:"last_recorded_at"`
	LastReceivedAt time.Time  `json:"last_received_at"`
	Batches        int64      `json:"batches"`
	Sessions       int64      `json:"sessions"`
}

type ReadingRow struct {
	Seq        uint32     `json:"seq"`
	Type       uint8      `json:"type"`
	Quality    uint8      `json:"quality"`
	Flags      uint8      `json:"flags"`
	RawTime    uint32     `json:"raw_time"`
	BootID     uint16     `json:"boot_id"`
	Value      float32    `json:"value"`
	RecordedAt *time.Time `json:"recorded_at"`
	TimeSource string     `json:"time_source"`
	BatchID    string     `json:"batch_id"`
	ReceivedAt time.Time  `json:"received_at"`
}

type SessionRow struct {
	SessionID             string     `json:"session_id"`
	DeviceID              string     `json:"device_id"`
	PhoneID               string     `json:"phone_id"`
	PhoneTimeAtConnect    *time.Time `json:"phone_time_at_connect"`
	SensorUptimeAtConnect *uint32    `json:"sensor_uptime_at_connect"`
	SensorBootIDAtConnect *uint16    `json:"sensor_boot_id_at_connect"`
	FirstSeen             time.Time  `json:"first_seen"`
	LastSeen              time.Time  `json:"last_seen"`
}

type BatchRow struct {
	BatchID       string    `json:"batch_id"`
	SessionID     string    `json:"session_id"`
	DeviceID      string    `json:"device_id"`
	ReceivedAt    time.Time `json:"received_at"`
	RecordCount   int       `json:"record_count"`
	InsertedCount int       `json:"inserted_count"`
	RejectedCount int       `json:"rejected_count"`
}

type LegacyRow struct {
	ID        int64     `json:"id"`
	Node      string    `json:"node"`
	Type      string    `json:"type"`
	Depth     int       `json:"depth"`
	Firmware  string    `json:"firmware"`
	Value     float64   `json:"value"`
	Timestamp time.Time `json:"timestamp"`
}

type Store interface {
	Ping(ctx context.Context) error

	// UpsertSession records the session, updating the connect-time fields if
	// it already exists.
	UpsertSession(ctx context.Context, s Session) error

	// InsertReadings stores readings, ignoring any (device, seq) already held.
	// Returns how many were new.
	InsertReadings(ctx context.Context, readings []Reading) (int, error)

	// HeldSeqs returns, in ascending order, every seq the store holds for the
	// device within [minSeq, maxSeq].
	HeldSeqs(ctx context.Context, deviceID string, minSeq, maxSeq uint32) ([]uint32, error)

	RecordBatch(ctx context.Context, b Batch) error

	InsertLegacy(ctx context.Context, r LegacyReading) error

	// Read side for the admin page. Lists are newest first.
	ListDevices(ctx context.Context) ([]DeviceSummary, error)
	ListReadings(ctx context.Context, deviceID string, limit int) ([]ReadingRow, error)
	ListSessions(ctx context.Context, limit int) ([]SessionRow, error)
	ListBatches(ctx context.Context, limit int) ([]BatchRow, error)
	ListLegacy(ctx context.Context, limit int) ([]LegacyRow, error)
}
