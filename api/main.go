package main

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net"
	"net/http"
	"os"
	"time"

	"github.com/jackc/pgx/v5"
	"github.com/joho/godotenv"
)

type SensorPayload struct {
	Node      string  `json:"node"`
	Depth     int     `json:"depth"`
	Firmware  string  `json:"firmware"`
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
		log.Printf("invalid JSON: %v", err)
		http.Error(w, "invalid JSON", http.StatusBadRequest)
		return
	}

	if payload.Timestamp == 0 {
		payload.Timestamp = time.Now().Unix()
	}
	ts := time.Unix(payload.Timestamp, 0)

	_, err := db.Exec(context.Background(),
		`INSERT INTO incoming_raw (node, type, depth, firmware, value, timestamp) VALUES ($1, $2, $3, $4, $5, $6)`,
		payload.Node, payload.Type, payload.Depth, payload.Firmware, payload.Value, ts)
	if err != nil {
		log.Printf("DB insert error: %v", err)
		http.Error(w, "db error", http.StatusInternalServerError)
		return
	}

	log.Printf("[DB] Saved: %+v", payload)
	w.WriteHeader(http.StatusAccepted)
	w.Write([]byte("ok"))
}

// Function to print all available network interfaces
func printNetworkInterfaces() {
	fmt.Println("=== Available Network Interfaces ===")
	interfaces, err := net.Interfaces()
	if err != nil {
		log.Printf("Error getting interfaces: %v", err)
		return
	}

	for _, iface := range interfaces {
		// Skip loopback and down interfaces
		if iface.Flags&net.FlagLoopback != 0 || iface.Flags&net.FlagUp == 0 {
			continue
		}

		addrs, err := iface.Addrs()
		if err != nil {
			continue
		}

		for _, addr := range addrs {
			if ipnet, ok := addr.(*net.IPNet); ok {
				if ipnet.IP.To4() != nil { // Only IPv4 addresses
					fmt.Printf("Interface: %s, IP: %s\n", iface.Name, ipnet.IP.String())
				}
			}
		}
	}
	fmt.Println("=====================================")
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

	http.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		fmt.Println("---> /")
		w.WriteHeader(http.StatusOK)
		w.Write([]byte("Hello, World!"))
	})
	http.HandleFunc("/test", func(w http.ResponseWriter, r *http.Request) {
		fmt.Println("---> /test")
		
		// Print request method and headers
		fmt.Printf("Method: %s\n", r.Method)
		fmt.Printf("Content-Type: %s\n", r.Header.Get("Content-Type"))
		fmt.Printf("Content-Length: %s\n", r.Header.Get("Content-Length"))
		
		// Read and print the request body
		if r.Body != nil {
			bodyBytes, err := io.ReadAll(r.Body)
			if err != nil {
				fmt.Printf("Error reading body: %v\n", err)
			} else {
				fmt.Printf("Body: %s\n", string(bodyBytes))
				
				// Try to parse as JSON and pretty print
				var jsonData interface{}
				if err := json.Unmarshal(bodyBytes, &jsonData); err == nil {
					prettyJSON, _ := json.MarshalIndent(jsonData, "", "  ")
					fmt.Printf("JSON Payload:\n%s\n", string(prettyJSON))
				}
			}
		}
		
		w.WriteHeader(http.StatusOK)
		w.Write([]byte("test successful"))
	})

	// Ingest handler
	http.HandleFunc("/ingest", ingestHandler)

	port := "8080"
	
	// Print available network interfaces
	printNetworkInterfaces()
	
	log.Println("🚀 Ingest server listening on port", port)
	log.Println("📡 ESP32 should use one of the IP addresses above with port", port)
	log.Fatal(http.ListenAndServe(":"+port, nil))
}
