package admin

import (
	"context"
	"encoding/json"
	"io"
	"log"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"api/internal/store"
)

func seeded(t *testing.T) *store.MemStore {
	t.Helper()
	st := store.NewMemStore()
	base := time.Date(2026, 9, 24, 3, 0, 0, 0, time.UTC)
	tick := 0
	st.Now = func() time.Time { tick++; return base.Add(time.Duration(tick) * time.Second) }
	ctx := context.Background()

	at := base
	_ = st.UpsertSession(ctx, store.Session{SessionID: "s1", DeviceID: "aabbccddeeff", PhoneID: "p1", PhoneTimeAtConnect: &at})
	_, _ = st.InsertReadings(ctx, []store.Reading{
		{DeviceID: "aabbccddeeff", Seq: 1, Type: 1, Value: 100, RecordedAt: &at, TimeSource: "sensor", BatchID: "b1"},
		{DeviceID: "aabbccddeeff", Seq: 2, Type: 1, Quality: 3, Value: -1, TimeSource: "unknown", BatchID: "b1"},
		{DeviceID: "aabbccddeeff", Seq: 3, Type: 2, Value: 3712, RecordedAt: &at, TimeSource: "phone_backfill", BatchID: "b1"},
	})
	// A separate call so this device's reading is received later.
	_, _ = st.InsertReadings(ctx, []store.Reading{
		{DeviceID: "112233445566", Seq: 9, Type: 1, Value: 5, TimeSource: "unknown", BatchID: "b2"},
	})
	_ = st.RecordBatch(ctx, store.Batch{BatchID: "b1", SessionID: "s1", DeviceID: "aabbccddeeff", RecordCount: 3, InsertedCount: 3})
	_ = st.RecordBatch(ctx, store.Batch{BatchID: "b2", SessionID: "s2", DeviceID: "112233445566", RecordCount: 1, InsertedCount: 1})
	_ = st.InsertLegacy(ctx, store.LegacyReading{Node: "soil-1", Type: "soil_resistance_ohms", Value: 5854, Timestamp: at})
	return st
}

func newMux(st store.Store, token string) *http.ServeMux {
	mux := http.NewServeMux()
	New(st, token, log.New(io.Discard, "", 0)).Register(mux)
	return mux
}

func get(t *testing.T, mux *http.ServeMux, path, token string) *httptest.ResponseRecorder {
	t.Helper()
	req := httptest.NewRequest(http.MethodGet, path, nil)
	if token != "" {
		req.Header.Set("Authorization", "Bearer "+token)
	}
	rr := httptest.NewRecorder()
	mux.ServeHTTP(rr, req)
	return rr
}

func TestOverviewListsDevicesNewestFirst(t *testing.T) {
	mux := newMux(seeded(t), "")
	rr := get(t, mux, "/v1/admin/overview", "")
	if rr.Code != http.StatusOK {
		t.Fatalf("%d %s", rr.Code, rr.Body.String())
	}
	var o Overview
	if err := json.Unmarshal(rr.Body.Bytes(), &o); err != nil {
		t.Fatal(err)
	}
	if o.ProtocolVersion != 1 || len(o.Devices) != 2 {
		t.Fatalf("%+v", o)
	}
	// Device 112233445566 received its reading last, so it sorts first.
	if o.Devices[0].DeviceID != "112233445566" || o.Devices[1].DeviceID != "aabbccddeeff" {
		t.Fatalf("order %s, %s", o.Devices[0].DeviceID, o.Devices[1].DeviceID)
	}
	d := o.Devices[1]
	if d.ReadingCount != 3 || d.MinSeq != 1 || d.MaxSeq != 3 || d.Batches != 1 || d.Sessions != 1 || d.LastRecordedAt == nil {
		t.Fatalf("%+v", d)
	}
	if len(o.Batches) != 2 || o.Batches[0].BatchID != "b2" {
		t.Fatalf("batches %+v", o.Batches)
	}
	if len(o.Sessions) != 1 || o.Sessions[0].SessionID != "s1" || o.Sessions[0].FirstSeen.IsZero() {
		t.Fatalf("sessions %+v", o.Sessions)
	}
	if len(o.Legacy) != 1 || o.Legacy[0].Node != "soil-1" {
		t.Fatalf("legacy %+v", o.Legacy)
	}
}

func TestReadingsNewestFirstWithNamesAndLimit(t *testing.T) {
	mux := newMux(seeded(t), "")
	rr := get(t, mux, "/v1/admin/devices/aabbccddeeff/readings?limit=2", "")
	if rr.Code != http.StatusOK {
		t.Fatalf("%d %s", rr.Code, rr.Body.String())
	}
	var r Readings
	if err := json.Unmarshal(rr.Body.Bytes(), &r); err != nil {
		t.Fatal(err)
	}
	if len(r.Readings) != 2 || r.Readings[0].Seq != 3 || r.Readings[1].Seq != 2 {
		t.Fatalf("%+v", r.Readings)
	}
	if r.Names.Types[2] != "battery_millivolts" || r.Names.Qualities[3] != "open" {
		t.Fatalf("names %+v", r.Names)
	}
	if rr := get(t, mux, "/v1/admin/devices/aabbccddeeff/readings?limit=zero", ""); rr.Code != http.StatusBadRequest {
		t.Fatalf("bad limit: %d", rr.Code)
	}
	if rr := get(t, mux, "/v1/admin/devices/nobody/readings", ""); rr.Code != http.StatusOK || !strings.Contains(rr.Body.String(), `"readings":[]`) {
		t.Fatalf("unknown device: %d %s", rr.Code, rr.Body.String())
	}
}

func TestTokenProtectsJSONButNotThePage(t *testing.T) {
	mux := newMux(seeded(t), "secret")
	if rr := get(t, mux, "/v1/admin/overview", ""); rr.Code != http.StatusUnauthorized {
		t.Fatalf("no token: %d", rr.Code)
	}
	if rr := get(t, mux, "/v1/admin/overview", "wrong"); rr.Code != http.StatusUnauthorized {
		t.Fatalf("wrong token: %d", rr.Code)
	}
	if rr := get(t, mux, "/v1/admin/overview", "secret"); rr.Code != http.StatusOK {
		t.Fatalf("right token: %d", rr.Code)
	}
	rr := get(t, mux, "/admin", "")
	if rr.Code != http.StatusOK || !strings.HasPrefix(rr.Header().Get("Content-Type"), "text/html") {
		t.Fatalf("page: %d %s", rr.Code, rr.Header().Get("Content-Type"))
	}
	if !strings.Contains(rr.Body.String(), "/v1/admin/overview") {
		t.Fatal("page does not reference the overview endpoint")
	}
}

func TestOpenWhenNoTokenConfigured(t *testing.T) {
	mux := newMux(seeded(t), "")
	if rr := get(t, mux, "/v1/admin/overview", ""); rr.Code != http.StatusOK {
		t.Fatalf("%d", rr.Code)
	}
}
