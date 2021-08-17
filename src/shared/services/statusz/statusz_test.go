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

package statusz_test

import (
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/shared/services/statusz"
)

func TestInstallPathHandler_NotReady(t *testing.T) {
	mux := http.NewServeMux()
	statusz.InstallPathHandler(mux, "/statusz/test", func() string {
		return "thisIsATest"
	})
	req, err := http.NewRequest("GET", "http://abc.com/statusz/test", nil)
	require.NoError(t, err)
	w := httptest.NewRecorder()
	mux.ServeHTTP(w, req)
	assert.Equal(t, http.StatusServiceUnavailable, w.Code)
	assert.Equal(t, w.Body.String(), "thisIsATest\n")
}

func TestInstallPathHandler_OK(t *testing.T) {
	mux := http.NewServeMux()
	statusz.InstallPathHandler(mux, "/statusz/test", func() string {
		return ""
	})
	req, err := http.NewRequest("GET", "http://abc.com/statusz/test", nil)
	require.NoError(t, err)
	w := httptest.NewRecorder()
	mux.ServeHTTP(w, req)
	assert.Equal(t, http.StatusOK, w.Code)
	assert.Equal(t, w.Body.String(), "\n")
}
