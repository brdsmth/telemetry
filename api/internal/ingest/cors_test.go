package ingest

import (
	"net/http"
	"net/http/httptest"
	"testing"
)

func TestCORSAnswersPreflightAndDecoratesResponses(t *testing.T) {
	inner := http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) { w.WriteHeader(http.StatusTeapot) })
	h := CORS(inner)

	rr := httptest.NewRecorder()
	h.ServeHTTP(rr, httptest.NewRequest(http.MethodOptions, "/v1/batches", nil))
	if rr.Code != http.StatusNoContent {
		t.Fatalf("preflight status %d", rr.Code)
	}
	if rr.Header().Get("Access-Control-Allow-Origin") != "*" || rr.Header().Get("Access-Control-Allow-Headers") == "" {
		t.Fatalf("preflight headers %v", rr.Header())
	}

	rr = httptest.NewRecorder()
	h.ServeHTTP(rr, httptest.NewRequest(http.MethodPost, "/v1/batches", nil))
	if rr.Code != http.StatusTeapot {
		t.Fatalf("inner handler not reached: %d", rr.Code)
	}
	if rr.Header().Get("Access-Control-Allow-Origin") != "*" {
		t.Fatal("response missing allow-origin")
	}
}
