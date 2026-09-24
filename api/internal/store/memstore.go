package store

import (
	"context"
	"sort"
	"sync"
	"time"
)

// MemStore is an in-memory Store for tests. It is safe for concurrent use.
type MemStore struct {
	mu       sync.Mutex
	Readings map[string]map[uint32]Reading // device -> seq -> reading
	received map[string]map[uint32]time.Time
	Sessions map[string]Session
	seen     map[string][2]time.Time // session -> first, last
	Batches  []Batch
	batchAt  []time.Time
	Legacy   []LegacyReading
	PingErr  error
	Now      func() time.Time
}

func NewMemStore() *MemStore {
	return &MemStore{
		Readings: map[string]map[uint32]Reading{},
		received: map[string]map[uint32]time.Time{},
		Sessions: map[string]Session{},
		seen:     map[string][2]time.Time{},
		Now:      time.Now,
	}
}

func (m *MemStore) Ping(context.Context) error { return m.PingErr }

func (m *MemStore) UpsertSession(_ context.Context, s Session) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	now := m.Now()
	if prev, ok := m.seen[s.SessionID]; ok {
		m.seen[s.SessionID] = [2]time.Time{prev[0], now}
	} else {
		m.seen[s.SessionID] = [2]time.Time{now, now}
	}
	m.Sessions[s.SessionID] = s
	return nil
}

func (m *MemStore) InsertReadings(_ context.Context, readings []Reading) (int, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	now := m.Now()
	inserted := 0
	for _, r := range readings {
		dev := m.Readings[r.DeviceID]
		if dev == nil {
			dev = map[uint32]Reading{}
			m.Readings[r.DeviceID] = dev
			m.received[r.DeviceID] = map[uint32]time.Time{}
		}
		if _, exists := dev[r.Seq]; exists {
			continue
		}
		dev[r.Seq] = r
		m.received[r.DeviceID][r.Seq] = now
		inserted++
	}
	return inserted, nil
}

func (m *MemStore) HeldSeqs(_ context.Context, deviceID string, minSeq, maxSeq uint32) ([]uint32, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	var out []uint32
	for seq := range m.Readings[deviceID] {
		if seq >= minSeq && seq <= maxSeq {
			out = append(out, seq)
		}
	}
	sort.Slice(out, func(i, j int) bool { return out[i] < out[j] })
	return out, nil
}

func (m *MemStore) RecordBatch(_ context.Context, b Batch) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.Batches = append(m.Batches, b)
	m.batchAt = append(m.batchAt, m.Now())
	return nil
}

func (m *MemStore) InsertLegacy(_ context.Context, r LegacyReading) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.Legacy = append(m.Legacy, r)
	return nil
}

func (m *MemStore) ListDevices(context.Context) ([]DeviceSummary, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	var out []DeviceSummary
	for dev, readings := range m.Readings {
		if len(readings) == 0 {
			continue
		}
		d := DeviceSummary{DeviceID: dev, ReadingCount: int64(len(readings))}
		first := true
		for seq, r := range readings {
			if first || seq < d.MinSeq {
				d.MinSeq = seq
			}
			if first || seq > d.MaxSeq {
				d.MaxSeq = seq
			}
			if r.RecordedAt != nil && (d.LastRecordedAt == nil || r.RecordedAt.After(*d.LastRecordedAt)) {
				t := *r.RecordedAt
				d.LastRecordedAt = &t
			}
			if at := m.received[dev][seq]; at.After(d.LastReceivedAt) {
				d.LastReceivedAt = at
			}
			first = false
		}
		for _, b := range m.Batches {
			if b.DeviceID == dev {
				d.Batches++
			}
		}
		for _, s := range m.Sessions {
			if s.DeviceID == dev {
				d.Sessions++
			}
		}
		out = append(out, d)
	}
	sort.Slice(out, func(i, j int) bool { return out[i].LastReceivedAt.After(out[j].LastReceivedAt) })
	return out, nil
}

func (m *MemStore) ListReadings(_ context.Context, deviceID string, limit int) ([]ReadingRow, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	var out []ReadingRow
	for seq, r := range m.Readings[deviceID] {
		out = append(out, ReadingRow{
			Seq: seq, Type: r.Type, Quality: r.Quality, Flags: r.Flags, RawTime: r.RawTime,
			BootID: r.BootID, Value: r.Value, RecordedAt: r.RecordedAt, TimeSource: r.TimeSource,
			BatchID: r.BatchID, ReceivedAt: m.received[deviceID][seq],
		})
	}
	sort.Slice(out, func(i, j int) bool { return out[i].Seq > out[j].Seq })
	if limit > 0 && len(out) > limit {
		out = out[:limit]
	}
	return out, nil
}

func (m *MemStore) ListSessions(_ context.Context, limit int) ([]SessionRow, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	var out []SessionRow
	for id, s := range m.Sessions {
		seen := m.seen[id]
		out = append(out, SessionRow{
			SessionID: id, DeviceID: s.DeviceID, PhoneID: s.PhoneID,
			PhoneTimeAtConnect: s.PhoneTimeAtConnect, SensorUptimeAtConnect: s.SensorUptimeAtConnect,
			SensorBootIDAtConnect: s.SensorBootIDAtConnect, FirstSeen: seen[0], LastSeen: seen[1],
		})
	}
	sort.Slice(out, func(i, j int) bool { return out[i].LastSeen.After(out[j].LastSeen) })
	if limit > 0 && len(out) > limit {
		out = out[:limit]
	}
	return out, nil
}

func (m *MemStore) ListBatches(_ context.Context, limit int) ([]BatchRow, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	var out []BatchRow
	for i := len(m.Batches) - 1; i >= 0; i-- {
		b := m.Batches[i]
		out = append(out, BatchRow{
			BatchID: b.BatchID, SessionID: b.SessionID, DeviceID: b.DeviceID, ReceivedAt: m.batchAt[i],
			RecordCount: b.RecordCount, InsertedCount: b.InsertedCount, RejectedCount: b.RejectedCount,
		})
		if limit > 0 && len(out) >= limit {
			break
		}
	}
	return out, nil
}

func (m *MemStore) ListLegacy(_ context.Context, limit int) ([]LegacyRow, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	var out []LegacyRow
	for i := len(m.Legacy) - 1; i >= 0; i-- {
		r := m.Legacy[i]
		out = append(out, LegacyRow{ID: int64(i + 1), Node: r.Node, Type: r.Type, Depth: r.Depth,
			Firmware: r.Firmware, Value: r.Value, Timestamp: r.Timestamp})
		if limit > 0 && len(out) >= limit {
			break
		}
	}
	return out, nil
}
