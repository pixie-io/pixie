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

	"px.dev/pixie/src/e2e_test/perf_tool/experimentpb"
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
			HTTPLoadTestWorkload(numConnections, targetRPS, true),
		},
		MetricSpecs: []*pb.MetricSpec{
			ProcessStatsMetrics(metricPeriod),
			// Stagger the second query a little bit because of query stability issues.
			HeapMetrics(metricPeriod + (2 * time.Second)),
			HTTPDataLossMetric(metricPeriod),
		},
		RunSpec: &pb.RunSpec{
			Actions: []*experimentpb.ActionSpec{
				{
					Type: experimentpb.START_VIZIER,
				},
				{
					Type: experimentpb.START_METRIC_RECORDERS,
				},
				{
					Type:     experimentpb.BURNIN,
					Duration: types.DurationProto(predeployDur),
				},
				{
					Type: experimentpb.START_WORKLOADS,
				},
				{
					Type:     experimentpb.RUN,
					Duration: types.DurationProto(dur),
				},
				{
					// Make sure metric recorders are stopped before vizier/workloads.
					Type: experimentpb.STOP_METRIC_RECORDERS,
				},
			},
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

// Perf test for px-k8ssandra
func K8ssandraExperiment(
	metricPeriod time.Duration,
	predeployDur time.Duration,
	dur time.Duration,
) *pb.ExperimentSpec {
	e := &pb.ExperimentSpec{
		VizierSpec: VizierWorkload(),
		WorkloadSpecs: []*pb.WorkloadSpec{
			K8ssandraWorkload(),
		},
		MetricSpecs: []*pb.MetricSpec{
			ProcessStatsMetrics(metricPeriod),
			// Stagger the second query a little bit because of query stability issues.
			HeapMetrics(metricPeriod + (2 * time.Second)),
		},
		RunSpec: &pb.RunSpec{
			Actions: []*experimentpb.ActionSpec{
				{
					Type: experimentpb.START_VIZIER,
				},
				{
					Type: experimentpb.START_METRIC_RECORDERS,
				},
				{
					Type:     experimentpb.BURNIN,
					Duration: types.DurationProto(predeployDur),
				},
				{
					Type: experimentpb.START_WORKLOADS,
				},
				{
					Type:     experimentpb.RUN,
					Duration: types.DurationProto(dur),
				},
				{
					// Make sure metric recorders are stopped before vizier/workloads.
					Type: experimentpb.STOP_METRIC_RECORDERS,
				},
			},
		},
		ClusterSpec: DefaultCluster,
	}
	e = addTags(e,
		"workload/k8ssandra",
		"parameter/default/",
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
			Actions: []*experimentpb.ActionSpec{
				{
					Type: experimentpb.START_VIZIER,
				},
				{
					Type: experimentpb.START_METRIC_RECORDERS,
				},
				{
					Type:     experimentpb.BURNIN,
					Duration: types.DurationProto(predeployDur),
				},
				{
					Type: experimentpb.START_WORKLOADS,
				},
				{
					Type:     experimentpb.RUN,
					Duration: types.DurationProto(dur),
				},
				{
					// Make sure metric recorders are stopped before vizier/workloads.
					Type: experimentpb.STOP_METRIC_RECORDERS,
				},
			},
		},
		ClusterSpec: DefaultCluster,
	}
	e = addTags(e,
		"workload/sock-shop",
		"parameter/default/",
	)
	return e
}

// OnlineBoutiqueExperiment is an experiment that runs all of online boutique as a workload.
func OnlineBoutiqueExperiment(
	metricPeriod time.Duration,
	predeployDur time.Duration,
	dur time.Duration,
) *pb.ExperimentSpec {
	e := &pb.ExperimentSpec{
		VizierSpec: VizierWorkload(),
		WorkloadSpecs: []*pb.WorkloadSpec{
			OnlineBoutiqueWorkload(),
		},
		MetricSpecs: []*pb.MetricSpec{
			ProcessStatsMetrics(metricPeriod),
			// Stagger the second query a little bit because of query stability issues.
			HeapMetrics(metricPeriod + (2 * time.Second)),
		},
		RunSpec: &pb.RunSpec{
			Actions: []*experimentpb.ActionSpec{
				{
					Type: experimentpb.START_VIZIER,
				},
				{
					Type: experimentpb.START_METRIC_RECORDERS,
				},
				{
					Type:     experimentpb.BURNIN,
					Duration: types.DurationProto(predeployDur),
				},
				{
					Type: experimentpb.START_WORKLOADS,
				},
				{
					Type:     experimentpb.RUN,
					Duration: types.DurationProto(dur),
				},
				{
					// Make sure metric recorders are stopped before vizier/workloads.
					Type: experimentpb.STOP_METRIC_RECORDERS,
				},
			},
		},
		ClusterSpec: DefaultCluster,
	}
	e = addTags(e,
		"workload/online-boutique",
		"parameter/default/",
	)
	return e
}

// KafkaExperiment is an experiment that runs the kafka demo workload.
func KafkaExperiment(
	metricPeriod time.Duration,
	predeployDur time.Duration,
	dur time.Duration,
) *pb.ExperimentSpec {
	e := &pb.ExperimentSpec{
		VizierSpec: VizierWorkload(),
		WorkloadSpecs: []*pb.WorkloadSpec{
			KafkaWorkload(),
		},
		MetricSpecs: []*pb.MetricSpec{
			ProcessStatsMetrics(metricPeriod),
			// Stagger the second query a little bit because of query stability issues.
			HeapMetrics(metricPeriod + (2 * time.Second)),
		},
		RunSpec: &pb.RunSpec{
			Actions: []*experimentpb.ActionSpec{
				{
					Type: experimentpb.START_VIZIER,
				},
				{
					Type: experimentpb.START_METRIC_RECORDERS,
				},
				{
					Type:     experimentpb.BURNIN,
					Duration: types.DurationProto(predeployDur),
				},
				{
					Type: experimentpb.START_WORKLOADS,
				},
				{
					Type:     experimentpb.RUN,
					Duration: types.DurationProto(dur),
				},
				{
					// Make sure metric recorders are stopped before vizier/workloads.
					Type: experimentpb.STOP_METRIC_RECORDERS,
				},
			},
		},
		ClusterSpec: DefaultCluster,
	}
	e = addTags(e,
		"workload/kafka",
		"parameter/default/",
	)
	return e
}

// HTTPLoadApplicationOverheadExperiment is an experiment that runs a simple client/server http loadtest,
// measuring pixie's impact on the application's performance.
func HTTPLoadApplicationOverheadExperiment(
	numConnections int,
	targetRPS int,
	metricPeriod time.Duration,
) *pb.ExperimentSpec {
	burninDur := 5 * time.Minute
	vizierDur := 10 * time.Minute
	noVizierDur := vizierDur
	e := &pb.ExperimentSpec{
		VizierSpec: VizierWorkload(),
		WorkloadSpecs: []*pb.WorkloadSpec{
			HTTPLoadTestWorkload(numConnections, targetRPS, false),
		},
		MetricSpecs: []*pb.MetricSpec{
			addActionSelector(ProtocolLoadtestPromMetrics(metricPeriod), "no_vizier"),
		},
		RunSpec: &pb.RunSpec{
			Actions: []*experimentpb.ActionSpec{
				{
					Type: experimentpb.START_WORKLOADS,
				},
				{
					Type: experimentpb.START_METRIC_RECORDERS,
					Name: "no_vizier",
				},
				{
					Type:     experimentpb.BURNIN,
					Duration: types.DurationProto(burninDur),
				},
				{
					Type:     experimentpb.RUN,
					Duration: types.DurationProto(noVizierDur),
					Name:     "no_vizier",
				},
				{
					Type: experimentpb.START_VIZIER,
				},
				{
					Type:     experimentpb.BURNIN,
					Duration: types.DurationProto(burninDur),
				},
				{
					Type:     experimentpb.RUN,
					Duration: types.DurationProto(vizierDur),
					Name:     "with_vizier",
				},
				{
					// Make sure metric recorders are stopped before vizier/workloads.
					Type: experimentpb.STOP_METRIC_RECORDERS,
					Name: "no_vizier",
				},
			},
		},
		ClusterSpec: DefaultCluster,
	}
	e = addTags(e,
		"application_overhead",
		"workload/http-loadtest",
		fmt.Sprintf("parameter/num_conns/%d", numConnections),
		fmt.Sprintf("parameter/target_rps/%d", targetRPS),
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

func addActionSelector(m *pb.MetricSpec, selector string) *pb.MetricSpec {
	m.ActionSelector = selector
	return m
}
