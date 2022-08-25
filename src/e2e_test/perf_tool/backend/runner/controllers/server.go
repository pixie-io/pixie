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
	"context"
	"time"

	"cloud.google.com/go/bigquery"

	"px.dev/pixie/src/e2e_test/perf_tool/backend/runner/runnerpb"
)

// ResultRow represents a single datapoint for a single metric, to be stored in bigquery.
// Each result row is associated to a particular run of an experiment (via ExperimentID and RetryIdx).
type ResultRow struct {
	// ExperimentID is a string representation of a UUID.
	ExperimentID string    `bigquery:"experiment_id"`
	RetryIdx     int64     `bigquery:"retry_idx"`
	Timestamp    time.Time `bigquery:"timestamp"`
	Name         string    `bigquery:"name"`
	Value        float64   `bigquery:"value"`
	Tags         string    `bigquery:"tags"`
}

// SpecRow stores an experiments ExperimentSpec in bigquery, encoded as JSON.
// SpecRows are only written to bigquery on experiment success, so all results from failed attempts can be ignored by joining on the retryIdx.
type SpecRow struct {
	// ExperimentID is a string representation of a experiment UUID.
	ExperimentID string `bigquery:"experiment_id"`
	// RetryIdx is the index of this attempt at running the experiment.
	// This is used to ignore ResultRows from previous attempts.
	RetryIdx int64 `bigquery:"retry_idx"`
	// Spec is a json encoded `experimentpb.ExperimentSpec`
	Spec string `bigquery:"spec"`
}

// Server defines an gRPC server type.
type Server struct {
	results *bigquery.Table
	specs   *bigquery.Table
}

// NewServer creates GRPC handlers.
func NewServer(results *bigquery.Table, specs *bigquery.Table) *Server {
	return &Server{
		results: results,
		specs:   specs,
	}
}

// StartExperiment starts the running of the given experiment spec.
// It's assumed that the necessary preparation tasks have completed (eg. this experiment has been assigned a cluster, and the build has completed)
func (s *Server) StartExperiment(ctx context.Context, req *runnerpb.StartExperimentRequest) (*runnerpb.StartExperimentResponse, error) {
	return &runnerpb.StartExperimentResponse{}, nil
}
