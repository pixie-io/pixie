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

package controllers

import (
	"github.com/jmoiron/sqlx"
	"github.com/prometheus/client_golang/prometheus"
	log "github.com/sirupsen/logrus"
)

var (
	missingUpdateCount = prometheus.NewCounterVec(prometheus.CounterOpts{
		Name: "missing_update_count",
		Help: "Number of updates we think we might be missing for a given vizier.",
	}, []string{"shardID", "vizierID"})

	vizierUpdatedCounter = prometheus.NewCounter(
		prometheus.CounterOpts{
			Name: "vizier_updated",
			Help: "Number of viziers that were updated",
		})
)

func init() {
	prometheus.MustRegister(missingUpdateCount)
	prometheus.MustRegister(vizierUpdatedCounter)
}

const statusQuery = `
SELECT
  enumerated.shard_id,
  enumerated.status,
  COALESCE(count, 0) as count
FROM (
  SELECT
    LEFT(vizier_cluster_id::text, 2) AS shard_id,
    status,
    COUNT(1)
  FROM
    vizier_cluster_info AS info
  GROUP BY
    shard_id,
    status
  ) AS stats
RIGHT JOIN (
  SELECT
    RIGHT('0' || TO_HEX(generate_series), 2) AS shard_id,
    status
  FROM
    GENERATE_SERIES(0, 255)
  CROSS JOIN (
    VALUES
      ('UNKNOWN'),
      ('HEALTHY'),
      ('UNHEALTHY'),
      ('DISCONNECTED'),
      ('UPDATING'),
      ('CONNECTED'),
      ('UPDATE_FAILED'),
      ('DEGRADED')
    ) statuses(status)
  ) AS enumerated
ON
  stats.shard_id = enumerated.shard_id
  AND stats.status::text = enumerated.status;
`

type statusMetricsCollector struct {
	db         *sqlx.DB
	statusDesc *prometheus.Desc
}

// NewStatusMetricsCollector creats a new vizier status metrics prometheus collector.
func NewStatusMetricsCollector(db *sqlx.DB) prometheus.Collector {
	return &statusMetricsCollector{
		db: db,
		statusDesc: prometheus.NewDesc(
			"vzmgr_vizier_status",
			"Status of the viziers",
			[]string{"shard", "status"},
			nil),
	}
}

// Describe implements Collector.
func (c *statusMetricsCollector) Describe(ch chan<- *prometheus.Desc) {
	ch <- c.statusDesc
}

// Collect implements Collector.
func (c *statusMetricsCollector) Collect(ch chan<- prometheus.Metric) {
	rows, err := c.db.Queryx(statusQuery)
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
