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

package httpmiddleware_test

import (
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/shared/services/authcontext"
	"px.dev/pixie/src/shared/services/env"
	"px.dev/pixie/src/shared/services/httpmiddleware"
	"px.dev/pixie/src/utils/testingutils"
)

func TestWithBearerAuthMiddleware(t *testing.T) {
	viper.Set("jwt_signing_key", "jwt-key")
	e := env.New("withpixie.ai")

	tests := []struct {
		Name          string
		Authorization string
		Path          string

		ExpectAuthSuccess bool
		// Only valid if auth is successful.
		ExpectHandlerAuthError bool
		ExpectHandlerUserID    string
	}{
		{
			Name:          "Auth Success With Bearer",
			Authorization: "Bearer " + testingutils.GenerateTestJWTToken(t, "jwt-key"),
			Path:          "/api/users",

			ExpectAuthSuccess:      true,
			ExpectHandlerAuthError: false,
			ExpectHandlerUserID:    testingutils.TestUserID,
		},
		{
			Name:          "Auth Success With Bearer",
			Authorization: "bearer " + testingutils.GenerateTestJWTToken(t, "jwt-key"),
			Path:          "/api/users",

			ExpectAuthSuccess:      true,
			ExpectHandlerAuthError: false,
			ExpectHandlerUserID:    testingutils.TestUserID,
		},
		{
			Name: "/healthz auth bypass",
			Path: "/healthz",

			ExpectAuthSuccess: true,
			// Not actually authorized, just bypass.
			ExpectHandlerAuthError: true,
		},
		{
			Name: "/healthz/subpath auth bypass",
			Path: "/healthz/subpath",

			ExpectAuthSuccess: true,
			// Not actually authorized, just bypass.
			ExpectHandlerAuthError: true,
		},
		{
			Name:              "Bad Bearer",
			Path:              "/api/users",
			Authorization:     "Bearr " + testingutils.GenerateTestJWTToken(t, "jwt-key"),
			ExpectAuthSuccess: false,
		},
		{
			Name:              "Missing Authorization",
			Path:              "/api/users",
			ExpectAuthSuccess: false,
		},
		{
			Name:              "Bad Token",
			Path:              "/api/users",
			Authorization:     "Bearer badtoken",
			ExpectAuthSuccess: false,
		},
	}

	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			handlerCallCount := 0
			testHandler := func(w http.ResponseWriter, r *http.Request) {
				handlerCallCount++
				sCtx, err := authcontext.FromContext(r.Context())
				if !test.ExpectHandlerAuthError {
					require.NoError(t, err)
					assert.NotNil(t, sCtx)
					assert.Equal(t, test.ExpectHandlerUserID, sCtx.Claims.GetUserClaims().UserID)
				} else {
					assert.NotNil(t, err)
				}
				w.WriteHeader(http.StatusOK)
			}
			req, err := http.NewRequest("GET", test.Path, nil)
			if len(test.Authorization) > 0 {
				req.Header.Add("Authorization", test.Authorization)
			}
			require.NoError(t, err)
			rr := httptest.NewRecorder()

			handler := httpmiddleware.WithBearerAuthMiddleware(
				e, http.HandlerFunc(testHandler))
			handler.ServeHTTP(rr, req)
			if test.ExpectAuthSuccess {
				assert.Equal(t, http.StatusOK, rr.Code)
				assert.Equal(t, 1, handlerCallCount)
			} else {
				assert.Equal(t, http.StatusUnauthorized, rr.Code)
				assert.Equal(t, 0, handlerCallCount)
			}
		})
	}
}
