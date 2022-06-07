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

package types

import (
	"bytes"
	"encoding/json"
)

// StringData holds our table's string data, which is stored as bytes. This
// custom type is necessary so that marshalling string data to JSON is
// performed correctly.
type StringData []byte

// Bytes gets the bytes from the string data.
func (data StringData) Bytes() []byte {
	return []byte(data)
}

// Marshal marshals the string data.
func (data StringData) Marshal() ([]byte, error) {
	if len(data) == 0 {
		return nil, nil
	}
	return []byte(data), nil
}

// MarshalTo marshals the string data to bytes.
func (data StringData) MarshalTo(mData []byte) (int, error) {
	if len(data) == 0 {
		return 0, nil
	}
	copy(mData, data)
	return len(data), nil
}

// Unmarshal unmarshals the string data to bytes.
func (data *StringData) Unmarshal(mData []byte) error {
	if len(mData) == 0 {
		//nolint:ineffassign // This follows the gogoproto generated code conventions.
		data = nil
		return nil
	}
	id := StringData(make([]byte, len(mData)))
	copy(id, mData)
	*data = id
	return nil
}

// Size gets the size of the string data.
func (data *StringData) Size() int {
	if data == nil {
		return 0
	}
	return len(*data)
}

// MarshalJSON marshals the string data to a JSON format.
func (data StringData) MarshalJSON() ([]byte, error) {
	s := string(data)
	return json.Marshal(s)
}

// UnmarshalJSON unmarshals JSON data to bytes.
func (data *StringData) UnmarshalJSON(mData []byte) error {
	var s string
	err := json.Unmarshal(mData, &s)
	if err != nil {
		return err
	}
	*data = StringData(s)
	return nil
}

// Equal compares two string datas and returns if they are equal.
func (data StringData) Equal(other StringData) bool {
	return bytes.Equal(data[0:], other[0:])
}

// Compare compares two string datas.
func (data StringData) Compare(other StringData) int {
	return bytes.Compare(data[0:], other[0:])
}
