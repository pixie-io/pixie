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

package statusz

import (
	"net/http"

	log "github.com/sirupsen/logrus"
)

// statusCheckFn is the function that is called to determine the status for the statusz endpoint.
// A non-empty returned string is considered a non-ready status and considered a 503 ServiceUnavailable.
// An empty string is considered to be a 200 OK.
type statusCheckFn func() string

// mux is an interface describing the methods InstallHandler requires.
type mux interface {
	Handle(pattern string, handler http.Handler)
}

// InstallPathHandler registers the statusz checks under path.
// This function can only be called once per mux/path combo.
// Our use of the statusz endpoint is to return a simple human-readable string
// containing the reason for why the pod is in its current state.
func InstallPathHandler(mux mux, path string, statusChecker statusCheckFn) {
	log.Debug("Installing readyz checkers")
	mux.Handle(path, registerStatuszCheck(statusChecker))
}

func registerStatuszCheck(statusChecker statusCheckFn) http.HandlerFunc {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		status := statusChecker()
		if status == "" {
			w.WriteHeader(http.StatusOK)
		}
		http.Error(w, status, http.StatusServiceUnavailable)
	})
}
