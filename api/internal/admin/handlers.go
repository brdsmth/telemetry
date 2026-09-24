// Package admin serves a minimal read-only view of what the store holds:
// devices, their readings, sessions, batches and the legacy bench feed. One
// embedded HTML page polls the JSON endpoints.
//
//	GET /admin                              the page
//	GET /v1/admin/overview                  devices, recent sessions, batches, bench feed
//	GET /v1/admin/devices/{id}/readings     newest readings for one device (?limit=)
//
// When a token is configured the JSON endpoints require
// `Authorization: Bearer <token>`; the page asks for it and remembers it.
package admin

import (
	"crypto/subtle"
	_ "embed"
	"encoding/json"
	"log"
	"net/http"
	"strconv"
	"strings"
	"time"

	"api/internal/schema"
	"api/internal/store"
)

//go:embed admin.html
var pageHTML []byte

const (
	defaultReadings = 200
	maxReadings     = 2000
	recentLimit     = 50
)

type Handler struct {
	st     store.Store
	token  string
	logger *log.Logger
}

// New builds the handler. An empty token leaves the endpoints open, which is
// fine for a local run and wrong for a public deployment.
func New(st store.Store, token string, logger *log.Logger) *Handler {
	return &Handler{st: st, token: token, logger: logger}
}

func (h *Handler) Register(mux *http.ServeMux) {
	mux.HandleFunc("GET /admin", h.page)
	mux.HandleFunc("GET /admin/", h.page)
	mux.Handle("GET /v1/admin/overview", h.protect(h.overview))
	mux.Handle("GET /v1/admin/devices/{id}/readings", h.protect(h.readings))
}

func (h *Handler) page(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	w.Header().Set("Cache-Control", "no-store")
	_, _ = w.Write(pageHTML)
}

func (h *Handler) protect(next http.HandlerFunc) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if h.token != "" {
			got := strings.TrimPrefix(r.Header.Get("Authorization"), "Bearer ")
			if subtle.ConstantTimeCompare([]byte(got), []byte(h.token)) != 1 {
				w.Header().Set("WWW-Authenticate", `Bearer realm="admin"`)
				http.Error(w, "unauthorized", http.StatusUnauthorized)
				return
			}
		}
		w.Header().Set("Cache-Control", "no-store")
		next(w, r)
	})
}

// Overview is the payload of GET /v1/admin/overview.
type Overview struct {
	GeneratedAt     time.Time             `json:"generated_at"`
	ProtocolVersion int                   `json:"protocol_version"`
	Devices         []store.DeviceSummary `json:"devices"`
	Sessions        []store.SessionRow    `json:"sessions"`
	Batches         []store.BatchRow      `json:"batches"`
	Legacy          []store.LegacyRow     `json:"legacy"`
}

func (h *Handler) overview(w http.ResponseWriter, r *http.Request) {
	ctx := r.Context()
	out := Overview{GeneratedAt: time.Now().UTC(), ProtocolVersion: schema.ProtocolVersion}
	var err error
	if out.Devices, err = h.st.ListDevices(ctx); err != nil {
		h.fail(w, "devices", err)
		return
	}
	if out.Sessions, err = h.st.ListSessions(ctx, recentLimit); err != nil {
		h.fail(w, "sessions", err)
		return
	}
	if out.Batches, err = h.st.ListBatches(ctx, recentLimit); err != nil {
		h.fail(w, "batches", err)
		return
	}
	if out.Legacy, err = h.st.ListLegacy(ctx, recentLimit); err != nil {
		h.fail(w, "legacy", err)
		return
	}
	writeJSON(w, out)
}

// Readings is the payload of GET /v1/admin/devices/{id}/readings.
type Readings struct {
	DeviceID string             `json:"device_id"`
	Readings []store.ReadingRow `json:"readings"`
	Names    names              `json:"names"`
}

type names struct {
	Types     map[uint8]string `json:"types"`
	Qualities map[uint8]string `json:"qualities"`
}

func (h *Handler) readings(w http.ResponseWriter, r *http.Request) {
	id := r.PathValue("id")
	limit := defaultReadings
	if s := r.URL.Query().Get("limit"); s != "" {
		n, err := strconv.Atoi(s)
		if err != nil || n < 1 {
			http.Error(w, "limit must be a positive integer", http.StatusBadRequest)
			return
		}
		limit = n
	}
	if limit > maxReadings {
		limit = maxReadings
	}
	rows, err := h.st.ListReadings(r.Context(), id, limit)
	if err != nil {
		h.fail(w, "readings", err)
		return
	}
	if rows == nil {
		rows = []store.ReadingRow{}
	}
	out := Readings{DeviceID: id, Readings: rows, Names: names{
		Types:     map[uint8]string{},
		Qualities: map[uint8]string{},
	}}
	for _, row := range rows {
		out.Names.Types[row.Type] = schema.TypeName(row.Type)
		out.Names.Qualities[row.Quality] = schema.QualityName(row.Quality)
	}
	writeJSON(w, out)
}

func (h *Handler) fail(w http.ResponseWriter, what string, err error) {
	h.logger.Printf("admin %s: %v", what, err)
	http.Error(w, "store error", http.StatusInternalServerError)
}

func writeJSON(w http.ResponseWriter, v any) {
	w.Header().Set("Content-Type", "application/json")
	_ = json.NewEncoder(w).Encode(v)
}
