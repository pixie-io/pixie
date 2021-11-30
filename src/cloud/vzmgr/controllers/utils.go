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

	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/shared/cvmsgspb"
)

// PodStatuses Type to use in sqlx for the map of pod statuses.
type PodStatuses map[string]*cvmsgspb.PodStatus

// Value Returns a golang database/sql driver value for PodStatuses.
func (p PodStatuses) Value() (driver.Value, error) {
	res, err := json.Marshal(p)
	if err != nil {
		return res, err
	}
	return driver.Value(res), err
}

// Scan Scans the sqlx database type ([]bytes) into the PodStatuses type.
func (p *PodStatuses) Scan(src interface{}) error {
	switch jsonText := src.(type) {
	case []byte:
		err := json.Unmarshal(jsonText, p)
		if err != nil {
			return status.Error(codes.Internal, "could not unmarshal control plane pod statuses")
		}
	default:
		return status.Error(codes.Internal, "could not unmarshal control plane pod statuses")
	}

	return nil
}
