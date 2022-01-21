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
	"sync"

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

	vizierToCloudMsgCount = prometheus.NewCounterVec(prometheus.CounterOpts{
		Name: "vizier_to_cloud_msg_count",
		Help: "Number of messages from vizier to cloud.",
	}, []string{"vizier_id", "kind"})
	vizierToCloudMsgSizeDist = prometheus.NewHistogramVec(prometheus.HistogramOpts{
		Name:    "vizier_to_cloud_msg_size_dist",
		Help:    "Histogram for message size from cloud to vizier.",
		Buckets: msgHistBuckets,
	}, []string{"kind"})

	stanPublishCount = prometheus.NewCounterVec(prometheus.CounterOpts{
		Name: "stan_publish_count",
		Help: "Number of messages published to STAN for each vizier",
	}, []string{"vizier_id"})
	natsPublishCount = prometheus.NewCounterVec(prometheus.CounterOpts{
		Name: "nats_publish_count",
		Help: "Number of messages published to NATSfor each vizier",
	}, []string{"vizier_id"})

	// Descriptions used for per vizier bridge metrics.
	cloudToVizierNATSMsgQueueLenDesc = prometheus.NewDesc(
		"cloud_to_vizier_nats_msg_queue_len",
		"Message queue length of NATS from cloud to vizier.",
		[]string{"vizier_id"},
		nil)
	cloudToVizierGRPCMsgQueueLenDesc = prometheus.NewDesc(
		"cloud_to_vizier_grpc_msg_queue_len",
		"Message queue length of GRPC from cloud to vizier.",
		[]string{"vizier_id"},
		nil)
	vizierToCloudGRPCMsgQueueLenDesc = prometheus.NewDesc(
		"vizier_to_cloud_grpc_msg_queue_len",
		"Message queue length from vizier to cloud over GRPC.",
		[]string{"vizier_id"},
		nil)
)

func init() {
	prometheus.MustRegister(cloudToVizierMsgCount)
	prometheus.MustRegister(cloudToVizierMsgSizeDist)

	prometheus.MustRegister(vizierToCloudMsgCount)
	prometheus.MustRegister(vizierToCloudMsgSizeDist)

	prometheus.MustRegister(stanPublishCount)
	prometheus.MustRegister(natsPublishCount)
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

type natsBridgeMetricCollector struct {
	natsBridges sync.Map
	// We use mutex here to prevent a race between the Register and UnRegister.
	// We need to make sure the same value is getting deleted as was registered and
	// that we don't unregister the wrong collector. The last Register wins and we don't
	// check to make sure the id was not previously registered.
	mu sync.Mutex
}

func (n *natsBridgeMetricCollector) Register(b *NATSBridgeController) {
	n.mu.Lock()
	defer n.mu.Unlock()
	n.natsBridges.Store(b.clusterID.String(), b)
}

func (n *natsBridgeMetricCollector) Unregister(b *NATSBridgeController) {
	n.mu.Lock()
	defer n.mu.Unlock()
	cid := b.clusterID.String()
	val, _ := n.natsBridges.Load(cid)
	if val != b {
		return
	}
	n.natsBridges.Delete(cid)
}

// Describe implements Collector.
func (*natsBridgeMetricCollector) Describe(ch chan<- *prometheus.Desc) {
	ch <- cloudToVizierNATSMsgQueueLenDesc
	ch <- cloudToVizierGRPCMsgQueueLenDesc
	ch <- vizierToCloudGRPCMsgQueueLenDesc
}

// Collect implements Collector.
func (n *natsBridgeMetricCollector) Collect(ch chan<- prometheus.Metric) {
	n.natsBridges.Range(func(key, value interface{}) bool {
		b := value.(*NATSBridgeController)
		cid := b.clusterID.String()

		ch <- prometheus.MustNewConstMetric(
			cloudToVizierNATSMsgQueueLenDesc,
			prometheus.GaugeValue,
			float64(len(b.subCh)),
			cid)
		ch <- prometheus.MustNewConstMetric(
			cloudToVizierGRPCMsgQueueLenDesc,
			prometheus.GaugeValue,
			float64(len(b.grpcInCh)),
			cid)
		ch <- prometheus.MustNewConstMetric(
			vizierToCloudGRPCMsgQueueLenDesc,
			prometheus.GaugeValue,
			float64(len(b.grpcOutCh)),
			cid)

		return true
	})
}
