package common

import (
	"context"
	"net/http"
)

// WithEnvMiddleware is the HTTP middleware to inject service environment into the context.
func WithEnvMiddleware(env BaseEnver, next http.Handler) http.Handler {
	f := func(w http.ResponseWriter, r *http.Request) {
		ctx := context.WithValue(r.Context(), EnvKey, env)
		next.ServeHTTP(w, r.WithContext(ctx))
	}

	return http.HandlerFunc(f)
}
