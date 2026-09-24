package ingest

import (
	"bytes"
	"encoding/base64"
	"encoding/json"
	"io"
	"log"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"api/internal/schema"
	"api/internal/store"
)

const (
	testDevice  = "aabbccddeeff"
	testBatch   = "0d4a8a58-7c8f-4a49-9b4a-3b7e1f2d9c11"
	testSession = "7a1c2e3f-1234-4bcd-9ef0-0123456789ab"
	testPhone   = "c0ffee00-aaaa-4bbb-8ccc-dddddddddddd"
)

func quietLogger() *log.Logger { return log.New(io.Discard, "", 0) }

func rec(seq uint32, flags uint8, t uint32, boot uint16) string {
	r := schema.Record{Version: 1, Type: 1, Quality: 0, Flags: flags, Seq: seq, Time: t, Value: 1000, BootID: boot}
	b := schema.Encode(r)
	return base64.StdEncoding.EncodeToString(b[:])
}

func envelope(records ...string) Envelope {
	phoneTime := int64(1790121600)
	uptime := uint32(3600)
	boot := uint16(3)
	return Envelope{
		BatchID: testBatch, SessionID: testSession, PhoneID: testPhone, DeviceID: testDevice,
		ProtocolVersion: 1, PhoneTimeAtConnect: &phoneTime, SensorUptimeAtConnect: &uptime,
		SensorBootIDAtConnect: &boot, Records: records,
	}
}

func post(t *testing.T, h http.Handler, body any) (*httptest.ResponseRecorder, Response) {
	t.Helper()
	var buf bytes.Buffer
	if s, ok := body.(string); ok {
		buf.WriteString(s)
	} else if err := json.NewEncoder(&buf).Encode(body); err != nil {
		t.Fatal(err)
	}
	req := httptest.NewRequest(http.MethodPost, "/v1/batches", &buf)
	rr := httptest.NewRecorder()
	h.ServeHTTP(rr, req)
	var resp Response
	if rr.Code == http.StatusOK {
		if err := json.Unmarshal(rr.Body.Bytes(), &resp); err != nil {
			t.Fatalf("bad response json: %v: %s", err, rr.Body.String())
		}
	}
	return rr, resp
}

func TestBatchStoresRecordsAndAcksContiguousRange(t *testing.T) {
	st := store.NewMemStore()
	h := Batches(st, quietLogger())

	rr, resp := post(t, h, envelope(rec(1, 1, 1790121600, 3), rec(2, 1, 1790121660, 3), rec(3, 1, 1790121720, 3)))
	if rr.Code != http.StatusOK {
		t.Fatalf("status %d: %s", rr.Code, rr.Body.String())
	}
	if resp.BatchID != testBatch {
		t.Fatalf("batch_id %q", resp.BatchID)
	}
	if len(resp.Acked) != 1 || resp.Acked[0] != (Range{1, 3}) {
		t.Fatalf("acked %v", resp.Acked)
	}
	if len(resp.Rejected) != 0 {
		t.Fatalf("rejected %v", resp.Rejected)
	}
	if got := len(st.Readings[testDevice]); got != 3 {
		t.Fatalf("stored %d readings", got)
	}
	r := st.Readings[testDevice][2]
	if r.TimeSource != "sensor" || r.RecordedAt == nil || r.RecordedAt.Unix() != 1790121660 {
		t.Fatalf("reading 2 time: %+v", r)
	}
	if len(st.Batches) != 1 || st.Batches[0].InsertedCount != 3 {
		t.Fatalf("batches %+v", st.Batches)
	}
	if s, ok := st.Sessions[testSession]; !ok || s.DeviceID != testDevice || s.PhoneTimeAtConnect == nil {
		t.Fatalf("session %+v", st.Sessions)
	}
}

func TestBatchIsIdempotent(t *testing.T) {
	st := store.NewMemStore()
	h := Batches(st, quietLogger())
	env := envelope(rec(10, 1, 1790121600, 3), rec(11, 1, 1790121660, 3))

	_, first := post(t, h, env)
	_, second := post(t, h, env)
	if len(second.Acked) != 1 || second.Acked[0] != (Range{10, 11}) {
		t.Fatalf("second acked %v", second.Acked)
	}
	if len(first.Acked) != len(second.Acked) || first.Acked[0] != second.Acked[0] {
		t.Fatalf("responses differ: %v vs %v", first.Acked, second.Acked)
	}
	if got := len(st.Readings[testDevice]); got != 2 {
		t.Fatalf("stored %d readings after replay", got)
	}
	if st.Batches[1].InsertedCount != 0 {
		t.Fatalf("replay inserted %d", st.Batches[1].InsertedCount)
	}
}

func TestBatchRejectsBadRecordsAndReportsGaps(t *testing.T) {
	st := store.NewMemStore()
	h := Batches(st, quietLogger())

	good1 := rec(1, 1, 1790121600, 3)
	badCrc := []byte(good1)
	decoded, _ := base64.StdEncoding.DecodeString(good1)
	decoded[19] ^= 0x01
	badCrc = []byte(base64.StdEncoding.EncodeToString(decoded))

	_, resp := post(t, h, envelope(good1, string(badCrc), "not base64!", rec(4, 1, 1790121780, 3), rec(5, 1, 1790121840, 3)))
	if len(resp.Rejected) != 2 {
		t.Fatalf("rejected %v", resp.Rejected)
	}
	if resp.Rejected[0].Index != 1 || resp.Rejected[0].Reason != "bad_crc" {
		t.Fatalf("rejected[0] %v", resp.Rejected[0])
	}
	if resp.Rejected[1].Index != 2 || resp.Rejected[1].Reason != "bad_base64" {
		t.Fatalf("rejected[1] %v", resp.Rejected[1])
	}
	want := []Range{{1, 1}, {4, 5}}
	if len(resp.Acked) != 2 || resp.Acked[0] != want[0] || resp.Acked[1] != want[1] {
		t.Fatalf("acked %v", resp.Acked)
	}
}

func TestBatchAcksPreviouslyHeldSeqsInsideTheRange(t *testing.T) {
	st := store.NewMemStore()
	h := Batches(st, quietLogger())
	post(t, h, envelope(rec(2, 1, 1790121600, 3)))
	_, resp := post(t, h, envelope(rec(1, 1, 1790121540, 3), rec(3, 1, 1790121660, 3)))
	if len(resp.Acked) != 1 || resp.Acked[0] != (Range{1, 3}) {
		t.Fatalf("acked %v", resp.Acked)
	}
}

func TestBatchBackfillsTimeForSameBootUptimeRecords(t *testing.T) {
	st := store.NewMemStore()
	h := Batches(st, quietLogger())

	// phone_time 1790121600 at sensor uptime 3600, boot 3. A record at uptime
	// 600 in boot 3 happened 3000 s before connect. Boot 2 cannot be placed.
	post(t, h, envelope(rec(7, 0, 600, 3), rec(8, 0, 100, 2)))

	r7 := st.Readings[testDevice][7]
	if r7.TimeSource != "phone_backfill" || r7.RecordedAt == nil || r7.RecordedAt.Unix() != 1790121600-3600+600 {
		t.Fatalf("r7 %+v", r7)
	}
	r8 := st.Readings[testDevice][8]
	if r8.TimeSource != "unknown" || r8.RecordedAt != nil {
		t.Fatalf("r8 %+v", r8)
	}
	if r7.RawTime != 600 || r7.BootID != 3 {
		t.Fatalf("raw fields must be preserved: %+v", r7)
	}
}

func TestBatchWithoutConnectContextLeavesUptimeRecordsUnknown(t *testing.T) {
	st := store.NewMemStore()
	h := Batches(st, quietLogger())
	env := envelope(rec(1, 0, 600, 3))
	env.PhoneTimeAtConnect = nil
	post(t, h, env)
	if r := st.Readings[testDevice][1]; r.TimeSource != "unknown" {
		t.Fatalf("%+v", r)
	}
}

func TestBatchEmptyRecordsIsOkWithNoAcks(t *testing.T) {
	st := store.NewMemStore()
	h := Batches(st, quietLogger())
	rr, resp := post(t, h, envelope())
	if rr.Code != http.StatusOK || len(resp.Acked) != 0 {
		t.Fatalf("%d %v", rr.Code, resp.Acked)
	}
}

func TestBatchValidation(t *testing.T) {
	st := store.NewMemStore()
	h := Batches(st, quietLogger())

	cases := map[string]func(*Envelope){
		"bad batch id": func(e *Envelope) { e.BatchID = "nope" },
		"bad session":  func(e *Envelope) { e.SessionID = "" },
		"bad phone":    func(e *Envelope) { e.PhoneID = "123" },
		"bad device":   func(e *Envelope) { e.DeviceID = "AABBCCDDEEFF" },
		"bad protocol": func(e *Envelope) { e.ProtocolVersion = 2 },
		"too many":     func(e *Envelope) { e.Records = make([]string, MaxRecordsPerBatch+1) },
	}
	for name, mutate := range cases {
		env := envelope(rec(1, 1, 1790121600, 3))
		mutate(&env)
		rr, _ := post(t, h, env)
		if rr.Code != http.StatusBadRequest {
			t.Errorf("%s: status %d", name, rr.Code)
		}
	}
	if rr, _ := post(t, h, "{not json"); rr.Code != http.StatusBadRequest {
		t.Errorf("invalid json: status %d", rr.Code)
	}
	req := httptest.NewRequest(http.MethodGet, "/v1/batches", nil)
	rr := httptest.NewRecorder()
	h.ServeHTTP(rr, req)
	if rr.Code != http.StatusMethodNotAllowed {
		t.Errorf("GET: status %d", rr.Code)
	}
	if len(st.Readings) != 0 {
		t.Fatal("nothing should have been stored")
	}
}

func TestContiguousRanges(t *testing.T) {
	got := contiguousRanges([]uint32{1, 2, 3, 5, 7, 8})
	want := []Range{{1, 3}, {5, 5}, {7, 8}}
	if len(got) != len(want) {
		t.Fatalf("%v", got)
	}
	for i := range want {
		if got[i] != want[i] {
			t.Fatalf("%v", got)
		}
	}
	if len(contiguousRanges(nil)) != 0 {
		t.Fatal("nil should give empty")
	}
}

func TestLegacyIngestStoresAndAnswers202(t *testing.T) {
	st := store.NewMemStore()
	h := Legacy(st, quietLogger())
	body := `{"node":"soil-1","depth":0,"firmware":"sensor-0.3.0","timestamp":1790121600,"value":5854,"type":"soil_resistance_ohms"}`
	req := httptest.NewRequest(http.MethodPost, "/ingest", bytes.NewBufferString(body))
	rr := httptest.NewRecorder()
	h.ServeHTTP(rr, req)
	if rr.Code != http.StatusAccepted || rr.Body.String() != "ok" {
		t.Fatalf("%d %q", rr.Code, rr.Body.String())
	}
	if len(st.Legacy) != 1 || st.Legacy[0].Timestamp.Unix() != 1790121600 || st.Legacy[0].Value != 5854 {
		t.Fatalf("%+v", st.Legacy)
	}

	// timestamp 0 means "server time".
	req = httptest.NewRequest(http.MethodPost, "/ingest", bytes.NewBufferString(`{"node":"x","timestamp":0,"value":1,"type":"t"}`))
	rr = httptest.NewRecorder()
	h.ServeHTTP(rr, req)
	if time.Since(st.Legacy[1].Timestamp) > time.Minute {
		t.Fatalf("server time not applied: %v", st.Legacy[1].Timestamp)
	}
}

func TestHealthz(t *testing.T) {
	st := store.NewMemStore()
	rr := httptest.NewRecorder()
	Healthz(st).ServeHTTP(rr, httptest.NewRequest(http.MethodGet, "/healthz", nil))
	if rr.Code != http.StatusOK {
		t.Fatalf("%d", rr.Code)
	}
	st.PingErr = io.ErrUnexpectedEOF
	rr = httptest.NewRecorder()
	Healthz(st).ServeHTTP(rr, httptest.NewRequest(http.MethodGet, "/healthz", nil))
	if rr.Code != http.StatusServiceUnavailable {
		t.Fatalf("%d", rr.Code)
	}
}
