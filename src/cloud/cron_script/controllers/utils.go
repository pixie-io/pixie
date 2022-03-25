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
	"database/sql/driver"
	"encoding/json"

	"github.com/gofrs/uuid"
)

// ClusterIDs represents an array of cluster IDs.
type ClusterIDs []uuid.UUID

// Value Returns a golang database/sql driver value for ClusterIDs.
func (p ClusterIDs) Value() (driver.Value, error) {
	return json.Marshal(p)
}

// Scan Scans the sqlx database type ([]bytes) into the ClusterIds type.
func (p *ClusterIDs) Scan(src interface{}) error {
	var data []byte
	switch v := src.(type) {
	case string:
		data = []byte(v)
	case []byte:
		data = v
	default:
		return nil
	}
	return json.Unmarshal(data, p)
}
