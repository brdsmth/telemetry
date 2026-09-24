// The telemetry API: the system of record for sensor readings.
//
//	POST /v1/batches  upload envelope from a phone (schema/PROTOCOL.md §4)
//	POST /ingest      legacy one-reading JSON from the bench firmware
//	GET  /healthz     200 when Postgres answers
//	GET  /admin       minimal read-only page over the JSON under /v1/admin/
package main

import (
	"context"
	"log"
	"net/http"
	"os"
	"time"

	"github.com/joho/godotenv"

	"api/internal/admin"
	"api/internal/ingest"
	"api/internal/store"
)

func main() {
	if err := godotenv.Load(); err != nil {
		log.Println("no .env file, using environment variables")
	}
	logger := log.New(os.Stdout, "", log.LstdFlags|log.LUTC)

	dbURL := os.Getenv("DATABASE_URL")
	if dbURL == "" {
		logger.Fatal("DATABASE_URL not set")
	}

	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	st, err := store.NewPostgres(ctx, dbURL)
	if err != nil {
		logger.Fatalf("database: %v", err)
	}
	defer st.Close()
	if err := st.EnsureSchema(ctx); err != nil {
		logger.Fatalf("schema: %v", err)
	}
	cancel()

	mux := http.NewServeMux()
	mux.HandleFunc("/healthz", ingest.Healthz(st))
	mux.HandleFunc("/ingest", ingest.Legacy(st, logger))
	mux.HandleFunc("/v1/batches", ingest.Batches(st, logger))
	// ADMIN_TOKEN guards the admin JSON; leave it unset only for local runs.
	adminToken := os.Getenv("ADMIN_TOKEN")
	if adminToken == "" {
		logger.Println("ADMIN_TOKEN not set: admin endpoints are open")
	}
	admin.New(st, adminToken, logger).Register(mux)
	mux.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/" {
			http.NotFound(w, r)
			return
		}
		_, _ = w.Write([]byte("telemetry api"))
	})

	// Railway and most hosts inject PORT; fall back for local runs.
	port := os.Getenv("PORT")
	if port == "" {
		port = "8080"
	}
	srv := &http.Server{
		Addr:              ":" + port,
		Handler:           ingest.CORS(mux),
		ReadHeaderTimeout: 10 * time.Second,
		ReadTimeout:       30 * time.Second,
		WriteTimeout:      30 * time.Second,
	}
	logger.Printf("listening on :%s", port)
	logger.Fatal(srv.ListenAndServe())
}
