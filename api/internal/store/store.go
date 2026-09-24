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
}
