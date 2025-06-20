package main

import (
	"context"
	"encoding/json"
	"log"
	"net/http"
	"os"
	"time"

	"github.com/jackc/pgx/v5"
	"github.com/joho/godotenv"
)

type SensorPayload struct {
	NodeID    string  `json:"node_id"`
	Timestamp int64   `json:"timestamp"`
	Value     float64 `json:"value"`
	Type      string  `json:"type"`
}

var db *pgx.Conn

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

	if payload.Timestamp == 0 {
		payload.Timestamp = time.Now().Unix()
	}
	ts := time.Unix(payload.Timestamp, 0)

	_, err := db.Exec(context.Background(),
		`INSERT INTO sensors (node_id, type, value, timestamp) VALUES ($1, $2, $3, $4)`,
		payload.NodeID, payload.Type, payload.Value, ts)
	if err != nil {
		log.Printf("DB insert error: %v", err)
		http.Error(w, "db error", http.StatusInternalServerError)
		return
	}

	log.Printf("[DB] Saved: %+v", payload)
	w.WriteHeader(http.StatusAccepted)
	w.Write([]byte("ok"))
}

func main() {

	if err := godotenv.Load(); err != nil {
		log.Println("⚠️ .env file not found, using environment variables")
	}

	var err error
	dbURL := os.Getenv("DATABASE_URL")
	if dbURL == "" {
		log.Fatal("DATABASE_URL not set")
	}
	db, err = pgx.Connect(context.Background(), dbURL)
	if err != nil {
		log.Fatalf("Unable to connect to DB: %v", err)
	}
	defer db.Close(context.Background())

	http.HandleFunc("/ingest", ingestHandler)

	port := "8080"
	log.Println("🚀 Ingest server listening on port", port)
	log.Fatal(http.ListenAndServe(":"+port, nil))
}
