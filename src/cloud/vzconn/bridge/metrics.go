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

package bridge

import (
	"strings"

	"github.com/prometheus/client_golang/prometheus"
)

var msgHistBuckets = []float64{4 << 4, 4 << 6, 4 << 8, 4 << 10, 4 << 12, 4 << 14}

var (
	cloudToVizierMsgCount = prometheus.NewCounterVec(prometheus.CounterOpts{
		Name: "cloud_to_vizier_msg_count",
		Help: "Number of messages from cloud to vizier.",
	}, []string{"vizier_id", "kind"})
	cloudToVizierMsgSizeDist = prometheus.NewHistogramVec(prometheus.HistogramOpts{
		Name:    "cloud_to_vizier_msg_size_dist",
		Help:    "Histogram for message size from cloud to vizier.",
		Buckets: msgHistBuckets,
	}, []string{"kind"})
	cloudToVizierMsgQueueLen = prometheus.NewGaugeVec(prometheus.GaugeOpts{
		Name: "cloud_to_vizier_msg_queue_len",
		Help: "Message queue length from cloud to vizier.",
	}, []string{"vizier_id"})
	cloudToVizierGRPCMsgQueueLen = prometheus.NewGaugeVec(prometheus.GaugeOpts{
		Name: "cloud_to_vizier_grpc_msg_queue_len",
		Help: "Message queue length from cloud to vizier on the GRPC buffer.",
	}, []string{"vizier_id"})

	vizierToCloudMsgCount = prometheus.NewCounterVec(prometheus.CounterOpts{
		Name: "vizier_to_cloud_msg_count",
		Help: "Number of messages from vizier to cloud.",
	}, []string{"vizier_id", "kind"})
	vizierToCloudMsgSizeDist = prometheus.NewHistogramVec(prometheus.HistogramOpts{
		Name:    "vizier_to_cloud_msg_size_dist",
		Help:    "Histogram for message size from cloud to vizier.",
		Buckets: msgHistBuckets,
	}, []string{"kind"})
	vizierToCloudMsgQueueLen = prometheus.NewGaugeVec(prometheus.GaugeOpts{
		Name: "vizier_to_cloud_msg_queue_len",
		Help: "Message queue length from vizier to cloud.",
	}, []string{"vizier_id"})
)

func init() {
	prometheus.MustRegister(cloudToVizierMsgCount)
	prometheus.MustRegister(cloudToVizierMsgSizeDist)
	prometheus.MustRegister(cloudToVizierMsgQueueLen)
	prometheus.MustRegister(cloudToVizierGRPCMsgQueueLen)

	prometheus.MustRegister(vizierToCloudMsgCount)
	prometheus.MustRegister(vizierToCloudMsgSizeDist)
	prometheus.MustRegister(vizierToCloudMsgQueueLen)
}

func cleanVizierToCloudMessageKind(s string) string {
	// For query responses just return "reply".
	if strings.HasPrefix(s, "reply-") {
		return "reply"
	}
	// Drop everything after colon. These are nats message channels.
	if idx := strings.Index(s, ":"); idx != -1 {
		s = s[:idx]
	}
	return s
}

func cleanCloudToVizierMessageKind(s string) string {
	// Just keep the part after the last dot.
	if idx := strings.LastIndex(s, "."); idx != -1 {
		s = s[idx+1:]
	}
	return s
}
