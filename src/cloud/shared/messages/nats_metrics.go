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

package messages

import (
	"strings"

	"github.com/nats-io/nats.go"
	"github.com/prometheus/client_golang/prometheus"
	log "github.com/sirupsen/logrus"
)

// NatsErrorCounter is a counter for NATS errors.
type NatsErrorCounter struct {
	count *prometheus.CounterVec
}

// NewNatsErrorCounter creates a new counter for NATS errors.
func NewNatsErrorCounter() *NatsErrorCounter {
	c := &NatsErrorCounter{
		count: prometheus.NewCounterVec(prometheus.CounterOpts{
			Name: "nats_error_count",
			Help: "NATS message bus error",
		}, []string{"shardID", "vizierID", "messageKind", "errorKind"}),
	}
	prometheus.MustRegister(c.count)
	return c
}

// HandleNatsError handles the Nats error by logging and tracking metrics.
func (c *NatsErrorCounter) HandleNatsError(conn *nats.Conn, sub *nats.Subscription, err error) {
	if err != nil {
		log.WithError(err).
			WithField("Subject", sub.Subject).
			Error("Got NATS error")
	}
	shard, vizierID, messageType := extractShardMessageInfo(sub.Subject)
	switch err {
	case nats.ErrSlowConsumer:
		c.count.WithLabelValues(shard, vizierID, messageType, "ErrSlowConsumer").Inc()
	default:
		c.count.WithLabelValues(shard, vizierID, messageType, "ErrUnknown").Inc()
	}
}

func extractShardMessageInfo(subject string) (string, string, string) {
	vals := strings.Split(subject, ".")
	if len(vals) >= 4 {
		return vals[1], vals[2], vals[3]
	}
	return "", "", ""
}
