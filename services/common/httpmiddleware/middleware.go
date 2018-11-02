// Package httpmiddleware has common middleware utilities that we use across our services.
package httpmiddleware

import (
	"net/http"
	"strings"

	commonenv "pixielabs.ai/pixielabs/services/common/env"
	"pixielabs.ai/pixielabs/services/common/sessioncontext"
)

// WithNewSessionMiddleware adds a new sessioncontext to the request context.
func WithNewSessionMiddleware(next http.Handler) http.Handler {
	f := func(w http.ResponseWriter, r *http.Request) {
		newCtx := sessioncontext.NewContext(r.Context(), sessioncontext.New())
		next.ServeHTTP(w, r.WithContext(newCtx))
	}
	return http.HandlerFunc(f)
}

// WithBearerAuthMiddleware checks for valid bearer auth or rejects the request.
// Must have session context middleware inserted before.
func WithBearerAuthMiddleware(env commonenv.Env, next http.Handler) http.Handler {
	f := func(w http.ResponseWriter, r *http.Request) {
		bearerSchema := "Bearer "
		sCtx, err := sessioncontext.FromContext(r.Context())
		if err != nil {
			http.Error(w, "cannot get session context, likely a configuration error.", http.StatusInternalServerError)
			return
		}

		// Try to get creds from the request.
		authHeader := r.Header.Get("Authorization")
		if authHeader == "" {
			http.Error(w, "Authorization header is required", http.StatusUnauthorized)
			return
		}
		if !strings.HasPrefix(authHeader, bearerSchema) {
			http.Error(w, "Bearer authorization is required", http.StatusUnauthorized)
			return
		}
		err = sCtx.UseJWTAuth(env.JWTSigningKey(), authHeader[len(bearerSchema):])
		if err != nil {
			http.Error(w, "Invalid auth", http.StatusUnauthorized)
			return
		}

		if !sCtx.ValidUser() {
			http.Error(w, "Invalid user", http.StatusUnauthorized)
			return
		}
		// Valid Auth.
		next.ServeHTTP(w, r)
	}
	return http.HandlerFunc(f)
}
