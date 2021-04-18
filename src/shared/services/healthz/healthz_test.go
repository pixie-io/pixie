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

package healthz_test

import (
	"fmt"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/shared/services/healthz"
)

func TestInstallPathHandler(t *testing.T) {
	mux := http.NewServeMux()
	healthz.InstallPathHandler(mux, "/healthz/test")
	req, err := http.NewRequest("GET", "http://abc.com/healthz/test", nil)
	require.NoError(t, err)
	w := httptest.NewRecorder()
	mux.ServeHTTP(w, req)
	assert.Equal(t, http.StatusOK, w.Code)
	assert.True(t, strings.HasPrefix(w.Body.String(), "OK"))
}

func TestRegisterDefaultChecks(t *testing.T) {
	mux := http.NewServeMux()
	healthz.RegisterDefaultChecks(mux)

	req, err := http.NewRequest("GET", "http://abc.com/healthz", nil)
	require.NoError(t, err)
	w := httptest.NewRecorder()
	mux.ServeHTTP(w, req)
	assert.Equal(t, http.StatusOK, w.Code)
	assert.True(t, strings.HasPrefix(w.Body.String(), "OK"))

	req, err = http.NewRequest("GET", "http://abc.com/ping", nil)
	require.NoError(t, err)
	w = httptest.NewRecorder()
	mux.ServeHTTP(w, req)
	assert.Equal(t, http.StatusOK, w.Code)
	assert.True(t, strings.HasPrefix(w.Body.String(), "OK"))
}

func TestMultipleChecks(t *testing.T) {
	var checks []healthz.Checker
	checks = append(checks, healthz.NamedCheck("bad-check", func() error {
		return fmt.Errorf("failed check")
	}))
	checks = append(checks, healthz.NamedCheck("good", func() error {
		return nil
	}))

	tests := []struct {
		path                       string
		expectedResponseStartsWith string
		expectedStatus             int
	}{
		{"/healthz", "FAILED", http.StatusInternalServerError},
		{"/healthz/bad-check", "FAILED", http.StatusInternalServerError},
		{"/healthz/good", "OK", http.StatusOK},
	}

	for _, test := range tests {
		mux := http.NewServeMux()
		healthz.RegisterDefaultChecks(mux, checks...)
		req, err := http.NewRequest("GET", fmt.Sprintf("http://abc.com%s", test.path), nil)
		require.NoError(t, err)

		w := httptest.NewRecorder()
		mux.ServeHTTP(w, req)
		assert.Equal(t, test.expectedStatus, w.Code)
		assert.True(t, strings.HasPrefix(w.Body.String(), test.expectedResponseStartsWith))
	}
}
