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

package services

import (
	"net/http"
	"os"
	"time"

	log "github.com/sirupsen/logrus"
	"github.com/zenazn/goji/web/mutil"

	version "px.dev/pixie/src/shared/goversion"
)

func init() {
	if version.GetVersion().IsDev() {
		log.SetReportCaller(true)
	}
}

// SetupServiceLogging sets up a consistent logging env for all services.
func SetupServiceLogging() {
	// Setup logging.
	log.SetOutput(os.Stdout)
	log.SetLevel(log.InfoLevel)
}

// HTTPLoggingMiddleware is a middleware function used for logging HTTP requests.
func HTTPLoggingMiddleware(next http.Handler) http.Handler {
	f := func(w http.ResponseWriter, r *http.Request) {
		start := time.Now()
		lw := mutil.WrapWriter(w)
		next.ServeHTTP(lw, r)
		duration := time.Since(start)
		logFields := log.Fields{
			"req_method":  r.Method,
			"req_path":    r.URL.String(),
			"resp_time":   duration,
			"resp_code":   lw.Status(),
			"resp_status": http.StatusText(lw.Status()),
			"resp_size":   lw.BytesWritten(),
		}

		switch {
		case lw.Status() != http.StatusOK && lw.Status() != 0:
			log.WithTime(start).WithFields(logFields).Info("HTTP Request")
		case r.URL.String() == "/healthz":
			log.WithTime(start).WithFields(logFields).Trace("HTTP Request")
		default:
			log.WithTime(start).WithFields(logFields).Debug("HTTP Request")
		}
	}
	return http.HandlerFunc(f)
}
