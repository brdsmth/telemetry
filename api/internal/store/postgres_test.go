package store

import (
	"context"
	"fmt"
	"os"
	"testing"
	"time"
)

// Runs only when DATABASE_URL points at a Postgres (CI provides one, locally
// `docker compose up -d` and the compose defaults do). Skips otherwise so
// `go test ./...` stays green on a laptop without Docker.
func openTestPostgres(t *testing.T) *Postgres {
	t.Helper()
	url := os.Getenv("DATABASE_URL")
	if url == "" {
		t.Skip("DATABASE_URL not set")
	}
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	p, err := NewPostgres(ctx, url)
	if err != nil {
		t.Fatalf("connect: %v", err)
	}
	if err := p.Ping(ctx); err != nil {
		t.Skipf("postgres not reachable: %v", err)
	}
	if err := p.EnsureSchema(ctx); err != nil {
		t.Fatalf("schema: %v", err)
	}
	t.Cleanup(p.Close)
	return p
}

func uniqueDevice(t *testing.T) string {
	return fmt.Sprintf("%012x", time.Now().UnixNano()&0xffffffffffff)
}

func TestPostgresInsertIsIdempotentAndHeldSeqsIsOrdered(t *testing.T) {
	p := openTestPostgres(t)
	ctx := context.Background()
	dev := uniqueDevice(t)

	now := time.Now().UTC().Truncate(time.Second)
	rs := []Reading{
		{DeviceID: dev, Seq: 3, Type: 1, RawTime: 3, BootID: 1, Value: 3, TimeSource: "unknown", BatchID: "b1"},
		{DeviceID: dev, Seq: 1, Type: 1, RawTime: 1, BootID: 1, Value: 1, TimeSource: "unknown", BatchID: "b1"},
		{DeviceID: dev, Seq: 2, Type: 1, Flags: 1, RawTime: uint32(now.Unix()), BootID: 1, Value: 2,
			RecordedAt: &now, TimeSource: "sensor", BatchID: "b1"},
	}
	n, err := p.InsertReadings(ctx, rs)
	if err != nil || n != 3 {
		t.Fatalf("first insert: n=%d err=%v", n, err)
	}
	n, err = p.InsertReadings(ctx, rs)
	if err != nil || n != 0 {
		t.Fatalf("second insert should be a no-op: n=%d err=%v", n, err)
	}

	held, err := p.HeldSeqs(ctx, dev, 1, 10)
	if err != nil {
		t.Fatal(err)
	}
	if len(held) != 3 || held[0] != 1 || held[1] != 2 || held[2] != 3 {
		t.Fatalf("held = %v", held)
	}
	held, _ = p.HeldSeqs(ctx, dev, 2, 2)
	if len(held) != 1 || held[0] != 2 {
		t.Fatalf("held range = %v", held)
	}
}

func TestPostgresSessionAndBatchUpserts(t *testing.T) {
	p := openTestPostgres(t)
	ctx := context.Background()
	dev := uniqueDevice(t)
	sid := "session-" + dev
	uptime := uint32(3600)
	boot := uint16(7)
	when := time.Now().UTC().Truncate(time.Second)

	s := Session{SessionID: sid, DeviceID: dev, PhoneID: "phone-1",
		PhoneTimeAtConnect: &when, SensorUptimeAtConnect: &uptime, SensorBootIDAtConnect: &boot}
	if err := p.UpsertSession(ctx, s); err != nil {
		t.Fatal(err)
	}
	if err := p.UpsertSession(ctx, s); err != nil {
		t.Fatalf("second upsert: %v", err)
	}

	b := Batch{BatchID: "batch-" + dev, SessionID: sid, DeviceID: dev, RecordCount: 3, InsertedCount: 3}
	if err := p.RecordBatch(ctx, b); err != nil {
		t.Fatal(err)
	}
	b.InsertedCount = 0
	if err := p.RecordBatch(ctx, b); err != nil {
		t.Fatalf("second batch record: %v", err)
	}
}
