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
	// Necessary to use go:embed directive.
	_ "embed"
	"time"

	"github.com/gogo/protobuf/types"

	pb "px.dev/pixie/src/e2e_test/perf_tool/experimentpb"
)

//go:embed scripts/process_stats.pxl
var processStatsScript string

//go:embed scripts/heap_size.pxl
var heapSizeScript string

//go:embed scripts/http_data_loss.pxl
var httpDataLossScript string

// ProcessStatsMetrics adds a metric spec that collects process stats such as rss,vsize, and cpu_usage.
func ProcessStatsMetrics(period time.Duration) *pb.MetricSpec {
	return &pb.MetricSpec{
		MetricType: &pb.MetricSpec_PxL{
			PxL: &pb.PxLScriptSpec{
				Script:           processStatsScript,
				Streaming:        false,
				CollectionPeriod: types.DurationProto(period),
				TableOutputs: map[string]*pb.PxLScriptOutputList{
					"*": {
						Outputs: []*pb.PxLScriptOutputSpec{
							singleMetricOutputWithPodNodeName("rss"),
							singleMetricOutputWithPodNodeName("vsize"),
							singleMetricOutputWithPodNodeName("cpu_usage"),
						},
					},
				},
			},
		},
	}
}

// HeapMetrics collects metrics around heap usage and amount of data stored in the table store.
func HeapMetrics(period time.Duration) *pb.MetricSpec {
	return &pb.MetricSpec{
		MetricType: &pb.MetricSpec_PxL{
			PxL: &pb.PxLScriptSpec{
				Script:           heapSizeScript,
				Streaming:        false,
				CollectionPeriod: types.DurationProto(period),
				TableOutputs: map[string]*pb.PxLScriptOutputList{
					"*": {
						Outputs: []*pb.PxLScriptOutputSpec{
							singleMetricOutputWithPodNodeName("table_size"),
							singleMetricOutputWithPodNodeName("current_allocated_bytes", "heap_allocated_bytes"),
							singleMetricOutputWithPodNodeName("heap_size_bytes"),
							singleMetricOutputWithPodNodeName("free_bytes", "heap_free_bytes"),
						},
					},
				},
			},
		},
	}
}

// HTTPDataLossMetric adds a metric that tracks HTTP data loss based on the `X-Px-Seq-Id` header.
func HTTPDataLossMetric(outputPeriod time.Duration) *pb.MetricSpec {
	return &pb.MetricSpec{
		MetricType: &pb.MetricSpec_PxL{
			PxL: &pb.PxLScriptSpec{
				Script:    httpDataLossScript,
				Streaming: true,
				TemplateValues: map[string]string{
					"header_name": "X-Px-Seq-Id",
				},
				TableOutputs: map[string]*pb.PxLScriptOutputList{
					"*": {
						Outputs: []*pb.PxLScriptOutputSpec{
							{
								OutputSpec: &pb.PxLScriptOutputSpec_DataLossCounter{
									DataLossCounter: &pb.DataLossCounterOutput{
										TimestampCol: "timestamp",
										MetricName:   "http_data_loss",
										SeqIDCol:     "seq_id",
										OutputPeriod: types.DurationProto(outputPeriod),
									},
								},
							},
						},
					},
				},
			},
		},
	}
}

// ProtocolLoadtestPromMetrics adds metrics that scrapes prometheus metrics from the protocol loadtest server, collecting process data (cpu usage, rss, vsize).
func ProtocolLoadtestPromMetrics(scrapePeriod time.Duration) *pb.MetricSpec {
	return &pb.MetricSpec{
		MetricType: &pb.MetricSpec_Prom{
			Prom: &pb.PrometheusScrapeSpec{
				Namespace:       "px-protocol-loadtest",
				MatchLabelKey:   "name",
				MatchLabelValue: "server",
				Port:            8080,
				ScrapePeriod:    types.DurationProto(scrapePeriod),
				MetricNames: map[string]string{
					"process_cpu_seconds_total":     "cpu_seconds_counter",
					"process_resident_memory_bytes": "rss",
					"process_virtual_memory_bytes":  "vsize",
				},
			},
		},
	}
}

func singleMetricOutputWithPodNodeName(col string, newName ...string) *pb.PxLScriptOutputSpec {
	metricName := col
	if len(newName) > 0 {
		metricName = newName[0]
	}
	return &pb.PxLScriptOutputSpec{
		OutputSpec: &pb.PxLScriptOutputSpec_SingleMetric{
			SingleMetric: &pb.SingleMetricPxLOutput{
				TimestampCol: "timestamp",
				MetricName:   metricName,
				ValueCol:     col,
				TagCols: []string{
					"node_name",
					"pod",
				},
			},
		},
	}
}
