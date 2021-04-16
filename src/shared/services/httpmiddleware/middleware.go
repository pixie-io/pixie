// Package httpmiddleware has services middleware utilities that we use across our services.
package httpmiddleware

import (
	"net/http"
	"strings"

	"px.dev/pixie/src/shared/services/authcontext"
	"px.dev/pixie/src/shared/services/env"
)

// GetTokenFromBearer extracts a bearer token from the authorization header.
func GetTokenFromBearer(r *http.Request) (string, bool) {
	bearerSchema := "Bearer "
	// Try to get creds from the request.
	authHeader := r.Header.Get("Authorization")
	if authHeader == "" {
		return "", false
	}

	if !strings.HasPrefix(authHeader, bearerSchema) {
		bearerSchema = "bearer " // GRPC includes bearer in header with lowercase.
	}

	if !strings.HasPrefix(authHeader, bearerSchema) {
		// Must have Bearer in authorization.
		return "", false
	}

	return authHeader[len(bearerSchema):], true
}

// WithBearerAuthMiddleware checks for valid bearer auth or rejects the request.
// This middleware should be use on all services (except auth/api) to validate our tokens.
func WithBearerAuthMiddleware(env env.Env, next http.Handler) http.Handler {
	f := func(w http.ResponseWriter, r *http.Request) {
		if strings.HasPrefix(r.URL.Path, "/healthz/") || r.URL.Path == "/healthz" {
			// Skip auth for healthcheck endpoints.
			next.ServeHTTP(w, r)
			return
		}
		token, ok := GetTokenFromBearer(r)
		if !ok {
			http.Error(w, "Must have bearer auth", http.StatusUnauthorized)
			return
		}

		aCtx := authcontext.New()
		err := aCtx.UseJWTAuth(env.JWTSigningKey(), token, env.Audience())
		if err != nil {
			http.Error(w, "Failed to parse token", http.StatusUnauthorized)
			return
		}

		if !aCtx.ValidClaims() {
			http.Error(w, "Invalid user", http.StatusUnauthorized)
			return
		}

		newCtx := authcontext.NewContext(r.Context(), aCtx)
		next.ServeHTTP(w, r.WithContext(newCtx))
	}
	return http.HandlerFunc(f)
}
