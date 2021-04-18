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

package handler_test

import (
	"errors"
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/shared/services/env"
	"px.dev/pixie/src/shared/services/handler"
)

func TestHandler_ServeHTTP(t *testing.T) {
	testHandler := func(e env.Env, w http.ResponseWriter, r *http.Request) error {
		return nil
	}
	h := handler.New(env.New("test"), testHandler)

	req, err := http.NewRequest("GET", "http://test.com/", nil)
	require.NoError(t, err)

	rw := httptest.NewRecorder()
	h.ServeHTTP(rw, req)
	assert.Equal(t, http.StatusOK, rw.Code)
}

func TestHandler_ServeHTTP_StatusError(t *testing.T) {
	testHandler := func(e env.Env, w http.ResponseWriter, r *http.Request) error {
		return &handler.StatusError{http.StatusUnauthorized, errors.New("badness")}
	}
	h := handler.New(env.New("test"), testHandler)

	req, err := http.NewRequest("GET", "http://test.com/", nil)
	require.NoError(t, err)

	rw := httptest.NewRecorder()
	h.ServeHTTP(rw, req)
	assert.Equal(t, http.StatusUnauthorized, rw.Code)
	assert.Equal(t, "badness\n", rw.Body.String())
}

func TestHandler_ServeHTTP_RegularError(t *testing.T) {
	testHandler := func(e env.Env, w http.ResponseWriter, r *http.Request) error {
		return errors.New("badness")
	}
	h := handler.New(env.New("test"), testHandler)

	req, err := http.NewRequest("GET", "http://test.com/", nil)
	require.NoError(t, err)

	rw := httptest.NewRecorder()
	h.ServeHTTP(rw, req)
	assert.Equal(t, http.StatusInternalServerError, rw.Code)
	assert.Equal(t, "Internal Server Error\n", rw.Body.String())
}
