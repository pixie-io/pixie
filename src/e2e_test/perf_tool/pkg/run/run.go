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
	"errors"
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

	clusterCtx          *cluster.Context
	clusterCleanup      func()
	vizier              deploy.Workload
	metricsResultCh     chan *metrics.ResultRow
	metricsBySelector   map[string][]metrics.Recorder
	workloadsBySelector map[string][]deploy.Workload

	// wg is for goroutines that are unrelated to the main execution of the experiment.
	wg sync.WaitGroup
	// eg is for goroutines that should fail the experiment if they return an error.
	eg *errgroup.Group
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

	r.metricsBySelector = make(map[string][]metrics.Recorder)
	r.metricsResultCh = make(chan *metrics.ResultRow)
	metricsChCloseOnce := sync.Once{}
	defer metricsChCloseOnce.Do(func() { close(r.metricsResultCh) })

	r.wg.Add(1)
	go r.runBQInserter(expID)

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

	var egCtx context.Context
	r.eg, egCtx = errgroup.WithContext(ctx)
	// errgroup.WithContext causes the egCtx to be cancelled when the first goroutine in the group errors.
	// We pass the group down. So, for example, if there's an error in one of the metric recorders' goroutines
	// it will cause context cancellation for the whole experiment,
	// allowing us to exit as soon as the error happens instead of waiting for the experiment to finish.
	r.eg.Go(func() error {
		return r.runActions(egCtx, spec)
	})

	if err := r.eg.Wait(); err != nil {
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

	metricsChCloseOnce.Do(func() { close(r.metricsResultCh) })
	r.wg.Wait()

	return nil
}

func (r *Runner) runActions(ctx context.Context, spec *experimentpb.ExperimentSpec) error {
	canceledErr := backoff.Permanent(context.Canceled)
	for _, a := range spec.RunSpec.Actions {
		log.Tracef("started action %s", experimentpb.ActionType_name[int32(a.Type)])
		if canceled := r.sendActionTimestamp(ctx, a, "begin"); canceled {
			return canceledErr
		}
		switch a.Type {
		case experimentpb.START_VIZIER:
			cleanup, err := r.startVizier(ctx, spec)
			if err != nil {
				return err
			}
			defer cleanup()
		case experimentpb.START_WORKLOADS:
			cleanup, err := r.startWorkloads(ctx, spec, a.Name)
			if err != nil {
				return err
			}
			defer cleanup()
		case experimentpb.START_METRIC_RECORDERS:
			cleanup, err := r.startMetricRecorders(ctx, spec, a.Name)
			if err != nil {
				return err
			}
			defer cleanup()
		case experimentpb.STOP_VIZIER:
			if err := r.stopVizier(); err != nil {
				return err
			}
		case experimentpb.STOP_WORKLOADS:
			if err := r.stopWorkloads(a.Name); err != nil {
				return err
			}
		case experimentpb.STOP_METRIC_RECORDERS:
			if err := r.stopMetricRecorders(a.Name); err != nil {
				return err
			}
		case experimentpb.RUN, experimentpb.BURNIN:
			dur, err := types.DurationFromProto(a.Duration)
			if err != nil {
				return err
			}
			log.WithField("duration", dur).
				WithField("action_name", a.Name).
				Infof("Waiting for %s action", experimentpb.ActionType_name[int32(a.Type)])
			if canceled := sleep(ctx, dur); canceled {
				return canceledErr
			}
		}
		log.Tracef("finished action %s", experimentpb.ActionType_name[int32(a.Type)])
		if canceled := r.sendActionTimestamp(ctx, a, "end"); canceled {
			return canceledErr
		}
	}
	return nil
}

func (r *Runner) startVizier(ctx context.Context, spec *experimentpb.ExperimentSpec) (func(), error) {
	log.Info("Deploying Vizier")
	noCleanup := func() {}
	if err := r.vizier.Start(r.clusterCtx); err != nil {
		return noCleanup, fmt.Errorf("failed to deploy vizier: %w", err)
	}

	log.Info("Waiting for Vizier HealthCheck")
	if err := r.vizier.WaitForHealthCheck(ctx, r.clusterCtx, spec.ClusterSpec); err != nil {
		_ = r.stopVizier()
		return noCleanup, err
	}
	return func() { _ = r.vizier.Close() }, nil
}

func (r *Runner) startMetricRecorders(ctx context.Context, spec *experimentpb.ExperimentSpec, selector string) (func(), error) {
	log.WithField("selector", selector).Infof("Starting metric recorders")
	noCleanup := func() {}
	for _, ms := range spec.MetricSpecs {
		if ms.ActionSelector != selector {
			continue
		}

		recorder := metrics.NewMetricsRecorder(r.pxCtx, r.clusterCtx, ms, r.eg, r.metricsResultCh)
		r.metricsBySelector[selector] = append(r.metricsBySelector[selector], recorder)
		if err := recorder.Start(ctx); err != nil {
			_ = r.stopMetricRecorders(selector)
			return noCleanup, fmt.Errorf("failed to start metrics recorder: %s", err)
		}
	}
	cleanup := func() { _ = r.stopMetricRecorders(selector) }
	return cleanup, nil
}

func (r *Runner) startWorkloads(ctx context.Context, spec *experimentpb.ExperimentSpec, selector string) (func(), error) {
	log.WithField("selector", selector).Info("Deploying workloads")
	noCleanup := func() {}
	for _, w := range r.workloadsBySelector[selector] {
		log.WithField("workload", w.Name()).Trace("deploying workload")
		if err := w.Start(r.clusterCtx); err != nil {
			_ = r.stopWorkloads(selector)
			return noCleanup, fmt.Errorf("failed to start workload deployment: %w", err)
		}
	}

	// Wait for workload healthchecks.
	eg := errgroup.Group{}
	for _, w := range r.workloadsBySelector[selector] {
		workload := w
		eg.Go(func() error {
			log.WithField("workload", workload.Name()).Trace("Waiting for workload healthcheck")
			if err := workload.WaitForHealthCheck(ctx, r.clusterCtx, spec.ClusterSpec); err != nil {
				return err
			}
			log.WithField("workload", workload.Name()).Trace("HealthCheck passed")
			return nil
		})
	}

	if err := eg.Wait(); err != nil {
		_ = r.stopWorkloads(selector)
		return noCleanup, err
	}
	cleanup := func() { _ = r.stopWorkloads(selector) }
	return cleanup, nil
}

func (r *Runner) stopVizier() error {
	log.Info("Stopping Vizier")
	return r.vizier.Close()
}

func (r *Runner) stopMetricRecorders(selector string) error {
	log.WithField("selector", selector).Info("Stopping metric recorders")
	mrs, ok := r.metricsBySelector[selector]
	if !ok {
		return nil
	}
	for _, mr := range mrs {
		mr.Close()
	}
	return nil
}

func (r *Runner) stopWorkloads(selector string) error {
	log.WithField("selector", selector).Info("Stopping workloads")
	ws, ok := r.workloadsBySelector[selector]
	if !ok {
		return nil
	}
	var errs []error
	for _, w := range ws {
		errs = append(errs, w.Close())
	}
	return errors.Join(errs...)
}

func (r *Runner) sendActionTimestamp(ctx context.Context, action *experimentpb.ActionSpec, prefix string) bool {
	actionName := strings.ToLower(experimentpb.ActionType_name[int32(action.Type)])
	name := fmt.Sprintf("%s_%s:%s", prefix, actionName, action.Name)
	row := &metrics.ResultRow{
		Timestamp: time.Now(),
		Name:      name,
		Value:     0.0,
	}
	select {
	case <-ctx.Done():
		return true
	case r.metricsResultCh <- row:
		return false
	}
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
	r.workloadsBySelector = make(map[string][]deploy.Workload)
	for _, s := range spec.WorkloadSpecs {
		w, err := deploy.NewWorkload(r.pxCtx, r.containerRegistryRepo, s)
		if err != nil {
			return err
		}
		log.Tracef("Preparing %s deployment", s.Name)
		if err := w.Prepare(); err != nil {
			return err
		}
		r.workloadsBySelector[s.ActionSelector] = append(r.workloadsBySelector[s.ActionSelector], w)
	}
	return nil
}

func (r *Runner) runBQInserter(expID uuid.UUID) {
	defer r.wg.Done()

	bqCh := make(chan interface{})
	defer close(bqCh)

	inserter := &bq.BatchInserter{
		Table:       r.resultTable,
		BatchSize:   512,
		PushTimeout: 2 * time.Minute,
	}
	go inserter.Run(bqCh)

	for row := range r.metricsResultCh {
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
