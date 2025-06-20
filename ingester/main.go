package main

import (
	"encoding/json"
	"log"
	"net/http"
	"time"
)

// SensorPayload is the expected structure of incoming sensor data
type SensorPayload struct {
	NodeID    string  `json:"node_id"`   // Unique ID per ESP32 node
	Timestamp int64   `json:"timestamp"` // UNIX time from device or gateway
	Value     float64 `json:"value"`     // Sensor reading
	Type      string  `json:"type"`      // e.g. "soil_moisture", "temperature"
}

func ingestHandler(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}

	var payload SensorPayload
	if err := json.NewDecoder(r.Body).Decode(&payload); err != nil {
		http.Error(w, "invalid JSON", http.StatusBadRequest)
		return
	}

	// Fallback if device didn’t send timestamp
	if payload.Timestamp == 0 {
		payload.Timestamp = time.Now().Unix()
	}

	log.Printf("[INGEST] Node=%s Type=%s Value=%.2f Time=%d",
		payload.NodeID, payload.Type, payload.Value, payload.Timestamp)

	w.WriteHeader(http.StatusAccepted)
	w.Write([]byte("ok"))
}

func main() {
	http.HandleFunc("/ingest", ingestHandler)

	port := "8080"
	log.Println("🚀 Ingest server listening on port", port)
	log.Fatal(http.ListenAndServe(":"+port, nil))
}
