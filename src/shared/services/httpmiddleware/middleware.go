/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// Package httpmiddleware has services middleware utilities that we use across our services.
package httpmiddleware

import (
	"fmt"
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

var healthEndpoints []string = []string{
	"/healthz", "/readyz", "/statusz",
}

func isHealthEndpoint(input string) bool {
	for _, healthEndpoint := range healthEndpoints {
		if strings.HasPrefix(input, fmt.Sprintf("%s/", healthEndpoint)) || input == healthEndpoint {
			return true
		}
	}
	return false
}

func isMetricsEndpoint(input string) bool {
	return strings.HasPrefix(input, "/metrics")
}

// WithBearerAuthMiddleware checks for valid bearer auth or rejects the request.
// This middleware should be use on all services (except auth/api) to validate our tokens.
func WithBearerAuthMiddleware(env env.Env, next http.Handler) http.Handler {
	f := func(w http.ResponseWriter, r *http.Request) {
		if isHealthEndpoint(r.URL.Path) {
			// Skip auth for healthcheck endpoints.
			next.ServeHTTP(w, r)
			return
		}
		if isMetricsEndpoint(r.URL.Path) {
			// Skip auth for metric endpoints.
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
