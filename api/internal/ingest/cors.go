package ingest

import "net/http"

// CORS lets a browser-hosted client (the app's web target during
// development) call the API from another origin. The endpoints it guards
// carry no credentials: the batch endpoint is open by design and the admin
// JSON uses a bearer token, which the browser sends only when the page
// supplies it.
func CORS(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		h := w.Header()
		h.Set("Access-Control-Allow-Origin", "*")
		h.Set("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
		h.Set("Access-Control-Allow-Headers", "Content-Type, Authorization")
		h.Set("Access-Control-Max-Age", "600")
		if r.Method == http.MethodOptions {
			w.WriteHeader(http.StatusNoContent)
			return
		}
		next.ServeHTTP(w, r)
	})
}
