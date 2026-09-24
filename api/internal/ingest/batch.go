// Package ingest holds the HTTP handlers. Batches implements the upload
// envelope from schema/PROTOCOL.md §4; Legacy keeps the bench firmware's
// one-reading JSON working.
package ingest

import (
	"context"
	"encoding/base64"
	"encoding/json"
	"errors"
	"log"
	"net/http"
	"regexp"
	"time"

	"api/internal/schema"
	"api/internal/store"
)

const MaxRecordsPerBatch = 5000

var (
	deviceIDPattern = regexp.MustCompile(`^[0-9a-f]{12}$`)
	uuidPattern     = regexp.MustCompile(`^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$`)
)

// Envelope is the request body of POST /v1/batches.
type Envelope struct {
	BatchID               string   `json:"batch_id"`
	SessionID             string   `json:"session_id"`
	PhoneID               string   `json:"phone_id"`
	DeviceID              string   `json:"device_id"`
	ProtocolVersion       int      `json:"protocol_version"`
	PhoneTimeAtConnect    *int64   `json:"phone_time_at_connect"`
	SensorUptimeAtConnect *uint32  `json:"sensor_uptime_at_connect"`
	SensorBootIDAtConnect *uint16  `json:"sensor_boot_id_at_connect"`
	Records               []string `json:"records"`
}

type Range struct {
	FromSeq uint32 `json:"from_seq"`
	ToSeq   uint32 `json:"to_seq"`
}

type Rejected struct {
	Index  int    `json:"index"`
	Reason string `json:"reason"`
}

// Response is the body of a 200 from POST /v1/batches.
type Response struct {
	BatchID  string     `json:"batch_id"`
	Acked    []Range    `json:"acked"`
	Rejected []Rejected `json:"rejected"`
}

// Batches returns the handler for POST /v1/batches.
func Batches(st store.Store, logger *log.Logger) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodPost {
			http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
			return
		}
		var env Envelope
		dec := json.NewDecoder(http.MaxBytesReader(w, r.Body, 4<<20))
		if err := dec.Decode(&env); err != nil {
			http.Error(w, "invalid json: "+err.Error(), http.StatusBadRequest)
			return
		}
		if err := validate(env); err != nil {
			http.Error(w, err.Error(), http.StatusBadRequest)
			return
		}

		resp, err := process(r.Context(), st, env)
		if err != nil {
			logger.Printf("batch device=%s session=%s batch=%s error=%v", env.DeviceID, env.SessionID, env.BatchID, err)
			http.Error(w, "store error", http.StatusInternalServerError)
			return
		}
		logger.Printf("batch device=%s session=%s batch=%s records=%d rejected=%d acked=%v",
			env.DeviceID, env.SessionID, env.BatchID, len(env.Records), len(resp.Rejected), resp.Acked)

		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusOK)
		_ = json.NewEncoder(w).Encode(resp)
	}
}

func validate(env Envelope) error {
	switch {
	case !uuidPattern.MatchString(env.BatchID):
		return errors.New("batch_id must be a uuid")
	case !uuidPattern.MatchString(env.SessionID):
		return errors.New("session_id must be a uuid")
	case env.PhoneID != "" && !uuidPattern.MatchString(env.PhoneID):
		return errors.New("phone_id must be a uuid when present")
	case !deviceIDPattern.MatchString(env.DeviceID):
		return errors.New("device_id must be 12 lowercase hex characters")
	case env.ProtocolVersion != schema.ProtocolVersion:
		return errors.New("unsupported protocol_version")
	case len(env.Records) > MaxRecordsPerBatch:
		return errors.New("too many records")
	}
	return nil
}

// process decodes, derives times, stores, and computes the ack ranges. Every
// step is idempotent so a retried batch produces the same response.
func process(ctx context.Context, st store.Store, env Envelope) (*Response, error) {
	session := store.Session{
		SessionID:             env.SessionID,
		DeviceID:              env.DeviceID,
		PhoneID:               env.PhoneID,
		SensorUptimeAtConnect: env.SensorUptimeAtConnect,
		SensorBootIDAtConnect: env.SensorBootIDAtConnect,
	}
	if env.PhoneTimeAtConnect != nil {
		t := time.Unix(*env.PhoneTimeAtConnect, 0).UTC()
		session.PhoneTimeAtConnect = &t
	}
	if err := st.UpsertSession(ctx, session); err != nil {
		return nil, err
	}

	resp := &Response{BatchID: env.BatchID, Acked: []Range{}, Rejected: []Rejected{}}
	var readings []store.Reading
	var minSeq, maxSeq uint32
	for i, b64 := range env.Records {
		raw, err := base64.StdEncoding.DecodeString(b64)
		if err != nil {
			resp.Rejected = append(resp.Rejected, Rejected{Index: i, Reason: "bad_base64"})
			continue
		}
		rec, err := schema.Decode(raw)
		if err != nil {
			resp.Rejected = append(resp.Rejected, Rejected{Index: i, Reason: err.Error()})
			continue
		}
		rd := toReading(env, rec)
		if len(readings) == 0 || rd.Seq < minSeq {
			minSeq = rd.Seq
		}
		if len(readings) == 0 || rd.Seq > maxSeq {
			maxSeq = rd.Seq
		}
		readings = append(readings, rd)
	}

	inserted, err := st.InsertReadings(ctx, readings)
	if err != nil {
		return nil, err
	}

	if len(readings) > 0 {
		held, err := st.HeldSeqs(ctx, env.DeviceID, minSeq, maxSeq)
		if err != nil {
			return nil, err
		}
		resp.Acked = contiguousRanges(held)
	}

	err = st.RecordBatch(ctx, store.Batch{
		BatchID:       env.BatchID,
		SessionID:     env.SessionID,
		DeviceID:      env.DeviceID,
		RecordCount:   len(env.Records),
		InsertedCount: inserted,
		RejectedCount: len(resp.Rejected),
	})
	return resp, err
}

// toReading applies the time reconstruction from schema/PROTOCOL.md §1.4.
func toReading(env Envelope, rec schema.Record) store.Reading {
	rd := store.Reading{
		DeviceID:   env.DeviceID,
		Seq:        rec.Seq,
		Type:       rec.Type,
		Quality:    rec.Quality,
		Flags:      rec.Flags,
		RawTime:    rec.Time,
		BootID:     rec.BootID,
		Value:      rec.Value,
		TimeSource: "unknown",
		BatchID:    env.BatchID,
	}
	switch {
	case rec.EpochValid():
		t := time.Unix(int64(rec.Time), 0).UTC()
		rd.RecordedAt = &t
		rd.TimeSource = "sensor"
	case env.PhoneTimeAtConnect != nil && env.SensorUptimeAtConnect != nil &&
		env.SensorBootIDAtConnect != nil && rec.BootID == *env.SensorBootIDAtConnect:
		epoch := *env.PhoneTimeAtConnect - int64(*env.SensorUptimeAtConnect) + int64(rec.Time)
		t := time.Unix(epoch, 0).UTC()
		rd.RecordedAt = &t
		rd.TimeSource = "phone_backfill"
	}
	return rd
}

// contiguousRanges collapses an ascending seq list into inclusive ranges.
func contiguousRanges(seqs []uint32) []Range {
	out := []Range{}
	for _, s := range seqs {
		n := len(out)
		if n > 0 && out[n-1].ToSeq+1 == s {
			out[n-1].ToSeq = s
			continue
		}
		out = append(out, Range{FromSeq: s, ToSeq: s})
	}
	return out
}
