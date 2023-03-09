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

package run

import (
	"encoding/json"
	"time"

	"github.com/gofrs/uuid"

	"px.dev/pixie/src/e2e_test/perf_tool/pkg/metrics"
)

// ResultRow represents a single datapoint for a single metric, to be stored in bigquery.
// Each result row is associated to a particular run of an experiment (via ExperimentID).
type ResultRow struct {
	// ExperimentID is a string representation of a UUID.
	ExperimentID string    `bigquery:"experiment_id"`
	Timestamp    time.Time `bigquery:"timestamp"`
	Name         string    `bigquery:"name"`
	Value        float64   `bigquery:"value"`
	// JSON encoded map[string]string of tags.
	Tags string `bigquery:"tags"`
}

// SpecRow stores an experiments ExperimentSpec in bigquery, encoded as JSON.
// SpecRows are only written to bigquery on experiment success, so all results from failed attempts can be ignored by joining on the ExperimentID
type SpecRow struct {
	// ExperimentID is a string representation of a experiment UUID.
	ExperimentID string `bigquery:"experiment_id"`
	// Spec is a json encoded `experimentpb.ExperimentSpec`
	Spec string `bigquery:"spec"`
	// CommitTopoOrder is the number of commits since the beginning of history for the commit this experiment was run on.
	// This is used to order experiments in datastudio views.
	CommitTopoOrder int `bigquery:"commit_topo_order"`
}

// MetricsRowToResultRow converts a `metrics.ResultRow` into a `bq.ResultRow`.
func MetricsRowToResultRow(expID uuid.UUID, row *metrics.ResultRow) (*ResultRow, error) {
	encodedTags, err := json.Marshal(row.Tags)
	if err != nil {
		return nil, err
	}
	return &ResultRow{
		ExperimentID: expID.String(),
		Timestamp:    row.Timestamp,
		Name:         row.Name,
		Value:        row.Value,
		Tags:         string(encodedTags),
	}, nil
}
