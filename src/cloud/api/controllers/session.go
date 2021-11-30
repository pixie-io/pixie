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

package controllers

import (
	"fmt"
	"net/http"

	"github.com/gorilla/sessions"

	"px.dev/pixie/src/cloud/api/apienv"
)

// GetDefaultSession loads the default session from the request. It will always return a valid session.
// If it returns an error, there was a problem decoding the previous session, or the previous
// session has expired.
func GetDefaultSession(env apienv.APIEnv, r *http.Request) (*sessions.Session, error) {
	store := env.CookieStore()
	session, err := store.Get(r, "default-session5")
	if err != nil {
		// CookieStore().Get(...) will always return a valid session, even if it has
		// errored trying to decode the previous existing session.
		return session, fmt.Errorf("error fetching session info: %v", err)
	}
	return session, nil
}
