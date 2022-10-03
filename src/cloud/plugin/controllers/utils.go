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
)

// Configurations type to use in sqlx for the map of configurations.
type Configurations map[string]string

// Value Returns a golang database/sql driver value for Configurations.
func (p Configurations) Value() (driver.Value, error) {
	res, err := json.Marshal(p)
	if err != nil {
		return res, err
	}
	return driver.Value(res), err
}

// Scan Scans the sqlx database type ([]bytes) into the Configurations type.
func (p *Configurations) Scan(src interface{}) error {
	switch jsonText := src.(type) {
	case []byte:
		err := json.Unmarshal(jsonText, p)
		if err != nil {
			return status.Error(codes.Internal, "could not unmarshal configurations")
		}
	default:
		return status.Error(codes.Internal, "could not unmarshal configurations")
	}

	return nil
}

// PresetScripts represents an array of PresetScripts.
type PresetScripts []*PresetScript

// PresetScript type to use in sqlx for preset scripts.
type PresetScript struct {
	Name              string `json:"name"`
	Description       string `json:"description"`
	DefaultFrequencyS int64  `json:"default_frequency_s" yaml:"defaultFrequencyS"`
	Script            string `json:"script"`
	DefaultDisabled   bool   `json:"default_disabled" yaml:"defaultDisabled"`
}

// Value Returns a golang database/sql driver value for PresetScripts.
func (p PresetScripts) Value() (driver.Value, error) {
	return json.Marshal(p)
}

// Scan Scans the sqlx database type ([]bytes) into the PresetScripts type.
func (p *PresetScripts) Scan(src interface{}) error {
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
