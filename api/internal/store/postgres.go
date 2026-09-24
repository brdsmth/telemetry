package store

import (
	"context"
	"fmt"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgxpool"
)

// Postgres implements Store on a pgx connection pool.
type Postgres struct {
	pool *pgxpool.Pool
}

func NewPostgres(ctx context.Context, url string) (*Postgres, error) {
	pool, err := pgxpool.New(ctx, url)
	if err != nil {
		return nil, fmt.Errorf("connect: %w", err)
	}
	return &Postgres{pool: pool}, nil
}

func (p *Postgres) Close() { p.pool.Close() }

func (p *Postgres) Ping(ctx context.Context) error { return p.pool.Ping(ctx) }

// EnsureSchema creates the tables if they are missing. Idempotent; runs on
// every start. A migrations tool takes over when a table needs altering.
func (p *Postgres) EnsureSchema(ctx context.Context) error {
	stmts := []string{
		// Legacy bench path.
		`CREATE TABLE IF NOT EXISTS incoming_raw (
			id        SERIAL PRIMARY KEY,
			node      TEXT NOT NULL,
			type      TEXT NOT NULL,
			depth     INT NOT NULL,
			firmware  TEXT NOT NULL,
			value     DOUBLE PRECISION NOT NULL,
			timestamp TIMESTAMPTZ NOT NULL DEFAULT now()
		)`,
		`CREATE TABLE IF NOT EXISTS sessions (
			session_id                TEXT PRIMARY KEY,
			device_id                 TEXT NOT NULL,
			phone_id                  TEXT,
			phone_time_at_connect     TIMESTAMPTZ,
			sensor_uptime_at_connect  BIGINT,
			sensor_boot_id_at_connect INT,
			first_seen                TIMESTAMPTZ NOT NULL DEFAULT now(),
			last_seen                 TIMESTAMPTZ NOT NULL DEFAULT now()
		)`,
		`CREATE TABLE IF NOT EXISTS batches (
			batch_id       TEXT PRIMARY KEY,
			session_id     TEXT NOT NULL,
			device_id      TEXT NOT NULL,
			received_at    TIMESTAMPTZ NOT NULL DEFAULT now(),
			record_count   INT NOT NULL,
			inserted_count INT NOT NULL,
			rejected_count INT NOT NULL
		)`,
		`CREATE TABLE IF NOT EXISTS readings (
			device_id   TEXT NOT NULL,
			seq         BIGINT NOT NULL,
			type        SMALLINT NOT NULL,
			quality     SMALLINT NOT NULL,
			flags       SMALLINT NOT NULL,
			raw_time    BIGINT NOT NULL,
			boot_id     INT NOT NULL,
			value       DOUBLE PRECISION NOT NULL,
			recorded_at TIMESTAMPTZ,
			time_source TEXT NOT NULL,
			batch_id    TEXT NOT NULL,
			received_at TIMESTAMPTZ NOT NULL DEFAULT now(),
			PRIMARY KEY (device_id, seq)
		)`,
		`CREATE INDEX IF NOT EXISTS readings_device_recorded_at ON readings (device_id, recorded_at)`,
	}
	for _, s := range stmts {
		if _, err := p.pool.Exec(ctx, s); err != nil {
			return fmt.Errorf("schema: %w", err)
		}
	}
	return nil
}

func (p *Postgres) UpsertSession(ctx context.Context, s Session) error {
	_, err := p.pool.Exec(ctx, `
		INSERT INTO sessions (session_id, device_id, phone_id, phone_time_at_connect,
		                      sensor_uptime_at_connect, sensor_boot_id_at_connect)
		VALUES ($1, $2, NULLIF($3, ''), $4, $5, $6)
		ON CONFLICT (session_id) DO UPDATE SET
			last_seen                 = now(),
			phone_time_at_connect     = COALESCE(EXCLUDED.phone_time_at_connect, sessions.phone_time_at_connect),
			sensor_uptime_at_connect  = COALESCE(EXCLUDED.sensor_uptime_at_connect, sessions.sensor_uptime_at_connect),
			sensor_boot_id_at_connect = COALESCE(EXCLUDED.sensor_boot_id_at_connect, sessions.sensor_boot_id_at_connect)`,
		s.SessionID, s.DeviceID, s.PhoneID, s.PhoneTimeAtConnect,
		nullableUint32(s.SensorUptimeAtConnect), nullableUint16(s.SensorBootIDAtConnect))
	return err
}

func (p *Postgres) InsertReadings(ctx context.Context, readings []Reading) (int, error) {
	if len(readings) == 0 {
		return 0, nil
	}
	batch := &pgx.Batch{}
	for _, r := range readings {
		batch.Queue(`
			INSERT INTO readings (device_id, seq, type, quality, flags, raw_time, boot_id,
			                      value, recorded_at, time_source, batch_id)
			VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11)
			ON CONFLICT (device_id, seq) DO NOTHING`,
			r.DeviceID, int64(r.Seq), int16(r.Type), int16(r.Quality), int16(r.Flags),
			int64(r.RawTime), int32(r.BootID), float64(r.Value), r.RecordedAt, r.TimeSource, r.BatchID)
	}
	results := p.pool.SendBatch(ctx, batch)
	defer results.Close()

	inserted := 0
	for range readings {
		tag, err := results.Exec()
		if err != nil {
			return inserted, fmt.Errorf("insert reading: %w", err)
		}
		inserted += int(tag.RowsAffected())
	}
	return inserted, nil
}

func (p *Postgres) HeldSeqs(ctx context.Context, deviceID string, minSeq, maxSeq uint32) ([]uint32, error) {
	rows, err := p.pool.Query(ctx,
		`SELECT seq FROM readings WHERE device_id = $1 AND seq BETWEEN $2 AND $3 ORDER BY seq`,
		deviceID, int64(minSeq), int64(maxSeq))
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var out []uint32
	for rows.Next() {
		var seq int64
		if err := rows.Scan(&seq); err != nil {
			return nil, err
		}
		out = append(out, uint32(seq))
	}
	return out, rows.Err()
}

func (p *Postgres) RecordBatch(ctx context.Context, b Batch) error {
	_, err := p.pool.Exec(ctx, `
		INSERT INTO batches (batch_id, session_id, device_id, record_count, inserted_count, rejected_count)
		VALUES ($1, $2, $3, $4, $5, $6)
		ON CONFLICT (batch_id) DO UPDATE SET
			received_at    = now(),
			record_count   = EXCLUDED.record_count,
			inserted_count = EXCLUDED.inserted_count,
			rejected_count = EXCLUDED.rejected_count`,
		b.BatchID, b.SessionID, b.DeviceID, b.RecordCount, b.InsertedCount, b.RejectedCount)
	return err
}

func (p *Postgres) InsertLegacy(ctx context.Context, r LegacyReading) error {
	_, err := p.pool.Exec(ctx,
		`INSERT INTO incoming_raw (node, type, depth, firmware, value, timestamp) VALUES ($1, $2, $3, $4, $5, $6)`,
		r.Node, r.Type, r.Depth, r.Firmware, r.Value, r.Timestamp)
	return err
}

func nullableUint32(v *uint32) *int64 {
	if v == nil {
		return nil
	}
	x := int64(*v)
	return &x
}

func nullableUint16(v *uint16) *int32 {
	if v == nil {
		return nil
	}
	x := int32(*v)
	return &x
}
