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

package deploy

import (
	"context"
	"time"

	log "github.com/sirupsen/logrus"
	"golang.org/x/sync/errgroup"

	"px.dev/pixie/src/e2e_test/perf_tool/experimentpb"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/cluster"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/deploy/checks"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/deploy/steps"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/pixie"
	"px.dev/pixie/src/utils/shared/k8s"
)

// Workload is the interface for workloads that get deployed to the experiment cluster.
type Workload interface {
	Name() string
	Prepare() error
	Start(*cluster.Context) error
	WaitForHealthCheck(context.Context, *cluster.Context, *experimentpb.ClusterSpec) error
	Close() error
}

type workloadImpl struct {
	name         string
	spec         *experimentpb.WorkloadSpec
	clusterCtx   *cluster.Context
	pxCtx        *pixie.Context
	deploySteps  []steps.DeployStep
	healthChecks []checks.HealthCheck

	namespacesToDelete map[string]bool
}

// NewWorkload creates a new Workload capable of deploying according to the spec given.
func NewWorkload(pxCtx *pixie.Context, containerRegistryRepo string, spec *experimentpb.WorkloadSpec) (Workload, error) {
	deploySteps := make([]steps.DeployStep, len(spec.DeploySteps))
	for i, stepSpec := range spec.DeploySteps {
		switch stepSpec.DeployType.(type) {
		case *experimentpb.DeployStep_Prerendered:
			deploySteps[i] = steps.NewPrerenderedDeploy(stepSpec.GetPrerendered())
		case *experimentpb.DeployStep_Skaffold:
			deploySteps[i] = steps.NewSkaffoldDeploy(stepSpec.GetSkaffold(), containerRegistryRepo)
		case *experimentpb.DeployStep_Px:
			deploySteps[i] = steps.NewPxDeploy(pxCtx, stepSpec.GetPx())
		}
	}
	// Add healthchecks from spec once those are implemented.
	healthchecks := make([]checks.HealthCheck, len(spec.Healthchecks))
	for i, checkSpec := range spec.Healthchecks {
		switch checkSpec.CheckType.(type) {
		case *experimentpb.HealthCheck_K8S:
			healthchecks[i] = checks.NewK8SHealthCheck(checkSpec.GetK8S())
		case *experimentpb.HealthCheck_PxL:
			healthchecks[i] = checks.NewPxLHealthCheck(pxCtx, checkSpec.GetPxL())
		}
	}

	return &workloadImpl{
		name:               spec.Name,
		spec:               spec,
		pxCtx:              pxCtx,
		deploySteps:        deploySteps,
		healthChecks:       healthchecks,
		namespacesToDelete: make(map[string]bool),
	}, nil
}

// Name returns the name of the workload.
func (w *workloadImpl) Name() string {
	return w.spec.Name
}

// Prepare the workload by building necessary images and rendering yamls.
func (w *workloadImpl) Prepare() error {
	log.WithField("workload", w.name).Trace("Preparing workload for deployment")
	for _, s := range w.deploySteps {
		log.WithField("workload", w.name).WithField("step", s.Name()).Trace("preparing deploy step")
		if err := s.Prepare(); err != nil {
			return err
		}
	}
	return nil
}

// Start starts the workload.
func (w *workloadImpl) Start(clusterCtx *cluster.Context) error {
	w.clusterCtx = clusterCtx
	log.WithField("workload", w.name).Trace("Starting workload deployment")
	for _, s := range w.deploySteps {
		log.WithField("workload", w.name).WithField("step", s.Name()).Trace("running deploy step")
		namespaces, err := s.Deploy(clusterCtx)
		if err != nil {
			return err
		}
		for _, ns := range namespaces {
			w.namespacesToDelete[ns] = true
		}
	}
	return nil
}

// WaitForHealthCheck waits for all of the workload's healthchecks to pass.
// The ClusterSpec is provided so that the healthcheck can use the number of nodes if necessary.
func (w *workloadImpl) WaitForHealthCheck(ctx context.Context, clusterCtx *cluster.Context, clusterSpec *experimentpb.ClusterSpec) error {
	var eg errgroup.Group
	for _, c := range w.healthChecks {
		check := c
		eg.Go(func() error {
			log.WithField("workload", w.name).WithField("check", check.Name()).Trace("waiting for healthcheck")
			err := check.Wait(ctx, clusterCtx, clusterSpec)
			if err != nil {
				return err
			}
			log.WithField("workload", w.name).WithField("check", check.Name()).Trace("healthcheck passed")
			return nil
		})
	}
	if err := eg.Wait(); err != nil {
		return err
	}
	return nil
}

// Close stops the workload.
func (w *workloadImpl) Close() error {
	// TODO(@benkilimnik): Run demo delete for demos that use CRDs (e.g. px-k8ssandra)
	// current approach can lead to orphaned resources or lingering finalizers that might complicate subsequent tests or deployments.
	for ns := range w.namespacesToDelete {
		log.WithField("workload", w.name).WithField("namespace", ns).Trace("deleting workload namespace")
		od := k8s.ObjectDeleter{
			Namespace:  ns,
			Clientset:  w.clusterCtx.Clientset(),
			RestConfig: w.clusterCtx.RestConfig(),
			Timeout:    5 * time.Minute,
		}
		return od.DeleteNamespace()
	}
	w.namespacesToDelete = make(map[string]bool)
	return nil
}
