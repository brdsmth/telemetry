package main

import (
	"context"
	"log"
	"os"

	"github.com/jackc/pgx/v5"
	"github.com/joho/godotenv"
)

func main() {
	if err := godotenv.Load(); err != nil {
		log.Println("⚠️ .env file not found, using environment variables")
	}

	dbURL := os.Getenv("DATABASE_URL")
	if dbURL == "" {
		log.Fatal("❌ DATABASE_URL not set")
	}

	conn, err := pgx.Connect(context.Background(), dbURL)
	if err != nil {
		log.Fatalf("❌ DB connect failed: %v", err)
	}
	defer conn.Close(context.Background())

	sql := `
	CREATE TABLE IF NOT EXISTS incoming_raw (
		id SERIAL PRIMARY KEY,
		node TEXT NOT NULL,
		type TEXT NOT NULL,
		depth INT NOT NULL, 
		value DOUBLE PRECISION NOT NULL,
		timestamp TIMESTAMPTZ NOT NULL DEFAULT now()
	)`
	if _, err := conn.Exec(context.Background(), sql); err != nil {
		log.Fatalf("❌ Table creation failed: %v", err)
	}

	log.Println("✅ sensors table created (or already exists)")
}
