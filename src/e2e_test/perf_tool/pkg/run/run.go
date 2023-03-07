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
	log "github.com/sirupsen/logrus"
	"golang.org/x/sync/errgroup"

	"px.dev/pixie/src/e2e_test/perf_tool/experimentpb"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/bq"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/cluster"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/deploy"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/pixie"
)

// Runner is responsible for running experiments using the ClusterProvider to get a cluster for the experiment.
type Runner struct {
	c                     cluster.Provider
	pxCtx                 *pixie.Context
	resultTable           *bq.Table
	specTable             *bq.Table
	containerRegistryRepo string

	clusterCtx     *cluster.Context
	clusterCleanup func()
	vizier         deploy.Workload
	workloads      []deploy.Workload
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
	eg := errgroup.Group{}
	eg.Go(func() error { return r.getCluster(ctx, spec.ClusterSpec) })
	eg.Go(func() error { return r.prepareWorkloads(ctx, spec) })

	if err := eg.Wait(); err != nil {
		if r.clusterCleanup != nil {
			r.clusterCleanup()
		}
		if r.clusterCtx != nil {
			r.clusterCtx.Close()
		}
		return err
	}
	defer r.clusterCleanup()
	defer r.clusterCtx.Close()
	return nil
}

func (r *Runner) getCluster(ctx context.Context, spec *experimentpb.ClusterSpec) error {
	log.Info("Getting cluster")
	clusterCtx, cleanup, err := r.c.GetCluster(ctx, spec)
	if err != nil {
		return err
	}
	r.clusterCtx = clusterCtx
	r.clusterCleanup = cleanup
	return nil
}

func (r *Runner) prepareWorkloads(ctx context.Context, spec *experimentpb.ExperimentSpec) error {
	vizier, err := deploy.NewWorkload(r.pxCtx, r.containerRegistryRepo, spec.VizierSpec)
	if err != nil {
		return err
	}
	r.vizier = vizier
	log.Trace("Preparing Vizier deployment")
	if err := r.vizier.Prepare(); err != nil {
		return err
	}
	workloads := make([]deploy.Workload, len(spec.WorkloadSpecs))
	for i, s := range spec.WorkloadSpecs {
		w, err := deploy.NewWorkload(r.pxCtx, r.containerRegistryRepo, s)
		if err != nil {
			return err
		}
		log.Tracef("Preparing %s deployment", s.Name)
		if err := w.Prepare(); err != nil {
			return err
		}
		workloads[i] = w
	}
	r.workloads = workloads
	return nil
}
