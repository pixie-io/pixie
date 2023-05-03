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
	"context"

	"golang.org/x/sync/errgroup"

	"px.dev/pixie/src/e2e_test/perf_tool/experimentpb"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/cluster"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/pixie"
)

// Recorder is an interface for all metric recorders.
type Recorder interface {
	Start(ctx context.Context) error
	Close()
}

// NewMetricsRecorder creates a new Recorder for the given MetricSpec.
func NewMetricsRecorder(pxCtx *pixie.Context, clusterCtx *cluster.Context, spec *experimentpb.MetricSpec, eg *errgroup.Group, resultCh chan<- *ResultRow) Recorder {
	switch spec.MetricType.(type) {
	case *experimentpb.MetricSpec_PxL:
		return &pxlScriptRecorderImpl{
			pxCtx: pxCtx,
			spec:  spec.GetPxL(),

			eg:       eg,
			resultCh: resultCh,
		}
	case *experimentpb.MetricSpec_Prom:
		return &prometheusRecorderImpl{
			clusterCtx: clusterCtx,
			spec:       spec.GetProm(),
			eg:         eg,
			resultCh:   resultCh,
		}
	}
	return nil
}
