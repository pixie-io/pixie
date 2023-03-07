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
	"context"

	"github.com/gofrs/uuid"

	"px.dev/pixie/src/e2e_test/perf_tool/experimentpb"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/bq"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/cluster"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/pixie"
)

// Runner is responsible for running experiments using the ClusterProvider to get a cluster for the experiment.
type Runner struct {
	c                     cluster.Provider
	pxCtx                 *pixie.Context
	resultTable           *bq.Table
	specTable             *bq.Table
	containerRegistryRepo string
}

// NewRunner creates a new Runner for the given contexts.
func NewRunner(c cluster.Provider, pxCtx *pixie.Context, resultTable *bq.Table, specTable *bq.Table, containerRegistryRepo string) *Runner {
	return &Runner{
		c:                     c,
		pxCtx:                 pxCtx,
		resultTable:           resultTable,
		specTable:             specTable,
		containerRegistryRepo: containerRegistryRepo,
	}
}

// RunExperiment runs an experiment according to the given ExperimentSpec.
func (r *Runner) RunExperiment(ctx context.Context, expID uuid.UUID, spec *experimentpb.ExperimentSpec) error {
	return nil
}
