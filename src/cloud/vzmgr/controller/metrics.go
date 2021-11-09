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

package controller

import (
	"github.com/jmoiron/sqlx"
	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/collectors"
	log "github.com/sirupsen/logrus"

	"px.dev/pixie/src/cloud/shared/vzshard"
)

const statusQuery = `
SELECT
  LEFT(vizier_cluster_id::text, 2) AS shard_id,
  status,
  COUNT(1)
FROM
  vizier_cluster_info
GROUP BY
  shard_id,
  status
HAVING
  LEFT(vizier_cluster_id::text, 2) IN (?);
`

func init() {
	prometheus.MustRegister(collectors.NewBuildInfoCollector())
}

type statusMetricsCollector struct {
	db         *sqlx.DB
	statusDesc *prometheus.Desc

	parsedQuery     string
	parsedShardArgs []interface{}
}

// NewStatusMetricsCollector creats a new vizier status metrics prometheus collector.
func NewStatusMetricsCollector(db *sqlx.DB) prometheus.Collector {
	query, args, err := sqlx.In(statusQuery, vzshard.GenerateShardRange())
	if err != nil {
		log.WithError(err).Fatal("Failed to create status query")
	}
	query = db.Rebind(query)
	return &statusMetricsCollector{
		db: db,
		statusDesc: prometheus.NewDesc(
			"vzmgr_vizier_status",
			"Status of the viziers",
			[]string{"shard", "status"},
			nil),

		parsedQuery:     query,
		parsedShardArgs: args,
	}
}

// Describe implements Collector.
func (c *statusMetricsCollector) Describe(ch chan<- *prometheus.Desc) {
	ch <- c.statusDesc
}

// Collect implements Collector.
func (c *statusMetricsCollector) Collect(ch chan<- prometheus.Metric) {
	rows, err := c.db.Queryx(c.parsedQuery, c.parsedShardArgs...)
	if err != nil {
		log.WithError(err).Warn("Failed to run status query")
		return
	}

	type statusInfo struct {
		ShardID string       `db:"shard_id"`
		Status  vizierStatus `db:"status"`
		Count   int64        `db:"count"`
	}

	defer rows.Close()
	for rows.Next() {
		s := &statusInfo{}
		err := rows.StructScan(s)
		if err != nil {
			log.WithError(err).Warn("Failed to scan struct from postgres")
			continue
		}
		ch <- prometheus.MustNewConstMetric(
			c.statusDesc,
			prometheus.GaugeValue,
			float64(s.Count),
			s.ShardID, s.Status.Stringify())
	}
}
