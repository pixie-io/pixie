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

package metrics

import (
	"fmt"
	"time"

	"px.dev/pixie/src/api/go/pxapi/types"
	"px.dev/pixie/src/api/proto/vizierpb"
)

func getColumnAsTime(r *types.Record, colName string, specName string) (time.Time, error) {
	d, err := getDatum(r, colName, specName)
	if err != nil {
		return time.Time{}, err
	}
	var val time.Time
	switch d.Type() {
	case vizierpb.TIME64NS:
		val = d.(*types.Time64NSValue).Value()
	case vizierpb.INT64:
		ns := d.(*types.Int64Value).Value()
		val = time.Unix(0, ns)
	default:
		return time.Time{}, fmt.Errorf(
			"'%s' specified a timestamp column named '%s', but that column is not a TIME64 or INT64 value",
			specName,
			colName,
		)
	}
	return val, nil
}

func getColumnAsFloat(r *types.Record, colName string, specName string) (float64, error) {
	d, err := getDatum(r, colName, specName)
	if err != nil {
		return 0.0, err
	}
	var val float64
	switch d.Type() {
	case vizierpb.INT64:
		val = float64(d.(*types.Int64Value).Value())
	case vizierpb.FLOAT64:
		val = d.(*types.Float64Value).Value()
	default:
		return 0.0, fmt.Errorf(
			"'%s' specified a numeric column name '%s', but that column is not a INT64 or FLOAT64 value",
			specName,
			colName,
		)
	}
	return val, nil
}

func getColumnAsInt64(r *types.Record, colName string, specName string) (int64, error) {
	d, err := getDatum(r, colName, specName)
	if err != nil {
		return 0.0, err
	}
	var val int64
	switch d.Type() {
	case vizierpb.INT64:
		val = d.(*types.Int64Value).Value()
	default:
		return 0.0, fmt.Errorf(
			"'%s' specified an int64 column named '%s', but that column is not an INT64 value",
			specName,
			colName,
		)
	}
	return val, nil
}

func getColumnAsString(r *types.Record, colName string, specName string) (string, error) {
	d, err := getDatum(r, colName, specName)
	if err != nil {
		return "", err
	}
	return d.String(), nil
}

func getDatum(r *types.Record, colName string, specName string) (types.Datum, error) {
	d := r.GetDatum(colName)
	if d == nil {
		return nil, fmt.Errorf("'%s' specified column '%s', but no such column in script output", specName, colName)
	}
	return d, nil
}
