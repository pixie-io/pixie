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

package checks

import (
	"context"
	"fmt"
	"time"

	"github.com/cenkalti/backoff/v4"
	log "github.com/sirupsen/logrus"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"

	"px.dev/pixie/src/e2e_test/perf_tool/experimentpb"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/cluster"
)

type k8sHealthCheck struct {
	spec *experimentpb.K8SPodsReadyCheck
}

var _ HealthCheck = &k8sHealthCheck{}

// NewK8SHealthCheck returns a new HealthCheck that checks that all pods in the given namespace are ready.
func NewK8SHealthCheck(spec *experimentpb.K8SPodsReadyCheck) HealthCheck {
	return &k8sHealthCheck{
		spec: spec,
	}
}

// Name returns a printable name for this healthcheck.
func (hc *k8sHealthCheck) Name() string {
	return fmt.Sprintf("pods in %s ready", hc.spec.Namespace)
}

// Wait waits for all pods in the spec-provided namespace to be ready.
func (hc *k8sHealthCheck) Wait(ctx context.Context, clusterCtx *cluster.Context, clusterSpec *experimentpb.ClusterSpec) error {
	expBackoff := backoff.NewExponentialBackOff()
	expBackoff.InitialInterval = time.Second
	expBackoff.MaxElapsedTime = 10 * time.Minute
	bo := backoff.WithContext(expBackoff, ctx)

	op := func() error {
		pl, err := clusterCtx.Clientset().CoreV1().Pods(hc.spec.Namespace).List(ctx, metav1.ListOptions{})
		if err != nil {
			return err
		}
		if len(pl.Items) == 0 {
			return fmt.Errorf(
				"no pods in namespace '%s', k8s pods ready healthcheck expects at least 1 pod",
				hc.spec.Namespace,
			)
		}
		for _, pod := range pl.Items {
			for _, cs := range pod.Status.InitContainerStatuses {
				if cs.State.Terminated == nil {
					return fmt.Errorf(
						"pod '%s' in namespace '%s' has unfinished init container '%s'",
						pod.Name,
						hc.spec.Namespace,
						cs.Name,
					)
				}
			}
			for _, cs := range pod.Status.ContainerStatuses {
				if !cs.Ready {
					return fmt.Errorf(
						"pod '%s' in namespace '%s' has not ready container '%s'",
						pod.Name,
						hc.spec.Namespace,
						cs.Name,
					)
				}
			}
		}
		return nil
	}
	notify := func(err error, dur time.Duration) {
		log.WithError(err).Tracef("failed k8s pods ready healthcheck, retrying in %v", dur.Round(time.Second))
	}
	return backoff.RetryNotify(op, bo, notify)
}
