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

package suites

import (
	"fmt"
	"time"

	"github.com/gogo/protobuf/types"

	pb "px.dev/pixie/src/e2e_test/perf_tool/experimentpb"
)

// HTTPLoadTestExperiment is an experiment that runs a simple client/server http loadtest.
func HTTPLoadTestExperiment(
	numConnections int,
	targetRPS int,
	metricPeriod time.Duration,
	predeployDur time.Duration,
	dur time.Duration,
) *pb.ExperimentSpec {
	e := &pb.ExperimentSpec{
		VizierSpec: VizierWorkload(),
		WorkloadSpecs: []*pb.WorkloadSpec{
			HTTPLoadTestWorkload(numConnections, targetRPS),
		},
		MetricSpecs: []*pb.MetricSpec{
			ProcessStatsMetrics(metricPeriod),
			// Stagger the second query a little bit because of query stability issues.
			HeapMetrics(metricPeriod + (2 * time.Second)),
			HTTPDataLossMetric(metricPeriod),
		},
		RunSpec: &pb.RunSpec{
			PreWorkloadDuration: types.DurationProto(predeployDur),
			Duration:            types.DurationProto(dur),
		},
		ClusterSpec: DefaultCluster,
	}
	e = addTags(e,
		"workload/http-loadtest",
		fmt.Sprintf("parameter/num_conns/%d", numConnections),
		fmt.Sprintf("parameter/target_rps/%d", targetRPS),
	)
	return e
}

// SockShopExperiment is an experiment that runs all of sock shop as a workload.
func SockShopExperiment(
	metricPeriod time.Duration,
	predeployDur time.Duration,
	dur time.Duration,
) *pb.ExperimentSpec {
	e := &pb.ExperimentSpec{
		VizierSpec: VizierWorkload(),
		WorkloadSpecs: []*pb.WorkloadSpec{
			SockShopWorkload(),
		},
		MetricSpecs: []*pb.MetricSpec{
			ProcessStatsMetrics(metricPeriod),
			// Stagger the second query a little bit because of query stability issues.
			HeapMetrics(metricPeriod + (2 * time.Second)),
		},
		RunSpec: &pb.RunSpec{
			PreWorkloadDuration: types.DurationProto(predeployDur),
			Duration:            types.DurationProto(dur),
		},
		ClusterSpec: DefaultCluster,
	}
	e = addTags(e,
		"workload/sock-shop",
		"parameter/default/",
	)
	return e
}

func addTags(e *pb.ExperimentSpec, tags ...string) *pb.ExperimentSpec {
	if e.Tags == nil {
		e.Tags = []string{}
	}
	e.Tags = append(e.Tags, tags...)
	return e
}
