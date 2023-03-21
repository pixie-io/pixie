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
	"bytes"
	"context"
	"fmt"
	"os/exec"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/cenkalti/backoff/v4"
	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/jsonpb"
	"github.com/gogo/protobuf/types"
	log "github.com/sirupsen/logrus"
	"golang.org/x/sync/errgroup"

	"px.dev/pixie/src/e2e_test/perf_tool/experimentpb"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/cluster"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/deploy"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/metrics"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/pixie"
	"px.dev/pixie/src/shared/bq"
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
	wg             sync.WaitGroup
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
	commitTopoOrder, err := getTopoOrder()
	if err != nil {
		return err
	}

	eg := errgroup.Group{}
	eg.Go(func() error { return r.getCluster(ctx, spec.ClusterSpec) })
	eg.Go(func() error {
		if err := r.prepareWorkloads(ctx, spec); err != nil {
			return backoff.Permanent(err)
		}
		return nil
	})

	metricsResultCh := make(chan *metrics.ResultRow)
	metricsChCloseOnce := sync.Once{}
	defer metricsChCloseOnce.Do(func() { close(metricsResultCh) })

	r.wg.Add(1)
	go r.runBQInserter(expID, metricsResultCh)

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

	log.Info("Deploying Vizier")
	if err := r.vizier.Start(r.clusterCtx); err != nil {
		return fmt.Errorf("failed to deploy vizier: %w", err)
	}
	defer r.vizier.Close()

	log.Info("Waiting for Vizier HealthCheck")
	if err := r.vizier.WaitForHealthCheck(ctx, r.clusterCtx, spec.ClusterSpec); err != nil {
		return err
	}

	log.Info("Starting metric recorders")
	// Start the metric recorders.
	mrs := make([]metrics.Recorder, len(spec.MetricSpecs))
	for i, ms := range spec.MetricSpecs {
		recorder := metrics.NewMetricsRecorder(r.pxCtx, ms, metricsResultCh)
		mrs[i] = recorder
		if err := recorder.Start(); err != nil {
			return fmt.Errorf("failed to start metrics recorder: %s", err)
		}
		defer recorder.Close()
	}

	// Wait for the PreWorkloadDuration specified in the RunSpec.
	// During this time, Vizier will be deployed, metrics are recording but no workloads are deployed.
	preWorkloadDur, err := types.DurationFromProto(spec.RunSpec.PreWorkloadDuration)
	if err != nil {
		return err
	}
	log.Infof("Waiting %v before deploying workloads", preWorkloadDur)
	if sleep(ctx, preWorkloadDur) {
		return nil
	}

	// Deploy the workloads.
	log.Info("Deploying workloads")
	for i, w := range r.workloads {
		log.WithField("workload", spec.WorkloadSpecs[i].Name).Trace("deploying workload")
		if err := w.Start(r.clusterCtx); err != nil {
			return fmt.Errorf("failed to start workload deployment: %w", err)
		}
		defer w.Close()
	}

	// Wait for workload healthchecks.
	eg = errgroup.Group{}
	for i, w := range r.workloads {
		name := spec.WorkloadSpecs[i].Name
		workload := w
		eg.Go(func() error {
			log.WithField("workload", name).Trace("Waiting for workload healthcheck")
			if err := workload.WaitForHealthCheck(ctx, r.clusterCtx, spec.ClusterSpec); err != nil {
				return err
			}
			log.WithField("workload", name).Trace("HealthCheck passed")
			return nil
		})
	}

	if err := eg.Wait(); err != nil {
		return err
	}

	// Add a row to the results with the time when workloads were deployed.
	row := &metrics.ResultRow{
		Timestamp: time.Now(),
		Name:      "workloads_deployed",
		Value:     0.0,
	}
	select {
	case <-ctx.Done():
	case metricsResultCh <- row:
	}

	// Wait for the experiment duration specified in the RunSpec.
	// During this time, Vizier and workloads are deployed and metrics are recording.
	dur, err := types.DurationFromProto(spec.RunSpec.Duration)
	if err != nil {
		return err
	}
	log.Infof("Experiment running for %v", dur)
	if sleep(ctx, dur) {
		return nil
	}
	log.Info("Experiment finished tearing down")

	// Teardown the experiment. First teardown metrics, then workloads, then the vizier.
	// This is a different order than the defers would occur.
	// Since all of the Close() methods should be idempotent, it should be fine that we call Close() twice.
	// We report the first error from any of the Close() methods, the defers will take care of tearing down the rest.
	log.Trace("Tearing down metric recorders")
	for _, recorder := range mrs {
		if err := recorder.Close(); err != nil {
			return err
		}
	}

	log.Trace("Tearing down workloads")
	for _, w := range r.workloads {
		if err := w.Close(); err != nil {
			return err
		}
	}

	log.Trace("Tearing down vizier")
	if err := r.vizier.Close(); err != nil {
		return err
	}

	// The experiment succeeded so we write the spec to bigquery.
	encodedSpec, err := (&jsonpb.Marshaler{}).MarshalToString(spec)
	if err != nil {
		return err
	}
	specRow := &SpecRow{
		ExperimentID:    expID.String(),
		Spec:            encodedSpec,
		CommitTopoOrder: commitTopoOrder,
	}

	inserter := r.specTable.Inserter()
	inserter.SkipInvalidRows = false

	putCtx, cancel := context.WithTimeout(ctx, 5*time.Minute)
	defer cancel()
	if err := inserter.Put(putCtx, specRow); err != nil {
		return err
	}

	metricsChCloseOnce.Do(func() { close(metricsResultCh) })
	r.wg.Wait()

	return nil
}

func sleep(ctx context.Context, dur time.Duration) bool {
	select {
	case <-ctx.Done():
		return true
	case <-time.After(dur):
		return false
	}
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

func (r *Runner) runBQInserter(expID uuid.UUID, resultCh <-chan *metrics.ResultRow) {
	defer r.wg.Done()

	bqCh := make(chan interface{})
	defer close(bqCh)

	inserter := &bq.BatchInserter{
		Table:       r.resultTable,
		BatchSize:   512,
		PushTimeout: 2 * time.Minute,
	}
	go inserter.Run(bqCh)

	for row := range resultCh {
		bqRow, err := MetricsRowToResultRow(expID, row)
		if err != nil {
			log.WithError(err).Error("Failed to convert result row")
			continue
		}
		bqCh <- bqRow
	}
}

func getTopoOrder() (int, error) {
	cmd := exec.Command("git", "rev-list", "--count", "HEAD")
	var stdout bytes.Buffer
	cmd.Stdout = &stdout
	if err := cmd.Run(); err != nil {
		return 0, err
	}
	return strconv.Atoi(strings.Trim(stdout.String(), " \n"))
}
