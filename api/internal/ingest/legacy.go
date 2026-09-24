package ingest

import (
	"encoding/json"
	"log"
	"net/http"
	"time"

	"api/internal/store"
)

// legacyPayload is what the bench firmware posts to /ingest.
type legacyPayload struct {
	Node      string  `json:"node"`
	Depth     int     `json:"depth"`
	Firmware  string  `json:"firmware"`
	Timestamp int64   `json:"timestamp"`
	Value     float64 `json:"value"`
	Type      string  `json:"type"`
}

// Legacy returns the handler for POST /ingest. It stores one reading per
// request in incoming_raw and answers 202, as the firmware expects.
func Legacy(st store.Store, logger *log.Logger) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodPost {
			http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
			return
		}
		var p legacyPayload
		if err := json.NewDecoder(http.MaxBytesReader(w, r.Body, 64<<10)).Decode(&p); err != nil {
			http.Error(w, "invalid JSON", http.StatusBadRequest)
			return
		}
		ts := time.Now().UTC()
		if p.Timestamp != 0 {
			ts = time.Unix(p.Timestamp, 0).UTC()
		}
		err := st.InsertLegacy(r.Context(), store.LegacyReading{
			Node: p.Node, Type: p.Type, Depth: p.Depth, Firmware: p.Firmware, Value: p.Value, Timestamp: ts,
		})
		if err != nil {
			logger.Printf("legacy ingest node=%s error=%v", p.Node, err)
			http.Error(w, "db error", http.StatusInternalServerError)
			return
		}
		logger.Printf("legacy ingest node=%s type=%s value=%g", p.Node, p.Type, p.Value)
		w.WriteHeader(http.StatusAccepted)
		_, _ = w.Write([]byte("ok"))
	}
}

// Healthz answers 200 when the store is reachable.
func Healthz(st store.Store) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		ctx, cancel := contextWithTimeout(r, 2*time.Second)
		defer cancel()
		if err := st.Ping(ctx); err != nil {
			http.Error(w, "db unreachable", http.StatusServiceUnavailable)
			return
		}
		w.WriteHeader(http.StatusOK)
		_, _ = w.Write([]byte("ok"))
	}
}
