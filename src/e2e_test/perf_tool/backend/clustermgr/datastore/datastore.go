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

package datastore

import "github.com/jmoiron/sqlx"

// Datastore implementation for runner using a PGSQL backend.
type Datastore struct {
	db *sqlx.DB
}

// NewDatastore creates a new cluster datastore instance implementing controllers.ClusterDatastore.
func NewDatastore(db *sqlx.DB) *Datastore {
	return &Datastore{
		db: db,
	}
}
