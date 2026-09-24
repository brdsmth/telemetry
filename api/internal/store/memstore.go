package store

import (
	"context"
	"sort"
	"sync"
)

// MemStore is an in-memory Store for tests. It is safe for concurrent use.
type MemStore struct {
	mu       sync.Mutex
	Readings map[string]map[uint32]Reading // device -> seq -> reading
	Sessions map[string]Session
	Batches  []Batch
	Legacy   []LegacyReading
	PingErr  error
}

func NewMemStore() *MemStore {
	return &MemStore{
		Readings: map[string]map[uint32]Reading{},
		Sessions: map[string]Session{},
	}
}

func (m *MemStore) Ping(context.Context) error { return m.PingErr }

func (m *MemStore) UpsertSession(_ context.Context, s Session) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.Sessions[s.SessionID] = s
	return nil
}

func (m *MemStore) InsertReadings(_ context.Context, readings []Reading) (int, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	inserted := 0
	for _, r := range readings {
		dev := m.Readings[r.DeviceID]
		if dev == nil {
			dev = map[uint32]Reading{}
			m.Readings[r.DeviceID] = dev
		}
		if _, exists := dev[r.Seq]; exists {
			continue
		}
		dev[r.Seq] = r
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
	return nil
}

func (m *MemStore) InsertLegacy(_ context.Context, r LegacyReading) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.Legacy = append(m.Legacy, r)
	return nil
}
