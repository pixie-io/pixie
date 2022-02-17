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

package formatters

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"

	"px.dev/pixie/src/api/go/pxapi/errdefs"
	"px.dev/pixie/src/api/go/pxapi/types"
)

const tableNameKey = "_tableName_"

// JSONFormatter formats data as JSON.
type JSONFormatter struct {
	w            io.Writer
	tableName    string
	headerValues []string
	encoder      *json.Encoder
}

type mapItem struct {
	Key, Value string
}

// mapSlice is an array of items.
type mapSlice []mapItem

// MarshalJSON marshals the k/v array in order.
func (ms mapSlice) MarshalJSON() ([]byte, error) {
	buf := &bytes.Buffer{}
	buf.Write([]byte{'{'})
	for i, mi := range ms {
		k, err := json.Marshal(&mi.Key)
		if err != nil {
			return nil, err
		}
		v, err := json.Marshal(&mi.Value)
		if err != nil {
			return nil, err
		}
		buf.Write(k)
		buf.Write([]byte{':'})
		buf.Write(v)
		if i < len(ms)-1 {
			buf.Write([]byte{','})
		}
	}
	buf.Write([]byte{'}'})
	return buf.Bytes(), nil
}

// JSONFormatterOption configures options on the formatter.
type JSONFormatterOption func(*JSONFormatter)

// NewJSONFormatter creates a JSONFormatter
func NewJSONFormatter(w io.Writer, opts ...JSONFormatterOption) (*JSONFormatter, error) {
	j := &JSONFormatter{
		w:       w,
		encoder: json.NewEncoder(w),
	}
	for _, opt := range opts {
		opt(j)
	}
	return j, nil
}

// HandleInit is called when the table metadata is available.
func (j *JSONFormatter) HandleInit(ctx context.Context, metadata types.TableMetadata) error {
	if len(j.tableName) != 0 {
		return fmt.Errorf("%w: did not expect init to be called more than once", errdefs.ErrInternalDuplicateTableMetadata)
	}
	j.tableName = metadata.Name
	for _, col := range metadata.ColInfo {
		j.headerValues = append(j.headerValues, col.Name)
	}
	return nil
}

// HandleRecord is called for each record of the table.
func (j *JSONFormatter) HandleRecord(ctx context.Context, record *types.Record) error {
	if len(record.Data) != len(j.headerValues) {
		return fmt.Errorf("%w: mismatch in header and data sizes", errdefs.ErrInvalidArgument)
	}

	var r []mapItem
	r = append(r, mapItem{
		Key:   tableNameKey,
		Value: j.tableName,
	})

	for i, d := range record.Data {
		r = append(r, mapItem{
			Key:   j.headerValues[i],
			Value: d.String(),
		})
	}

	return j.encoder.Encode(mapSlice(r))
}

// HandleDone is called when all data has been streamed.
func (j *JSONFormatter) HandleDone(ctx context.Context) error {
	return nil
}
