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
	"context"
	"encoding/json"
	"fmt"
	"math"
	"sync"
	"time"

	"px.dev/pixie/src/shared/bq"

	"cloud.google.com/go/bigquery"
	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"github.com/nats-io/nats.go"
	"github.com/prometheus/common/model"
	"github.com/prometheus/prometheus/model/timestamp"
	"github.com/prometheus/prometheus/prompb"
	log "github.com/sirupsen/logrus"

	"px.dev/pixie/src/cloud/shared/vzshard"
	"px.dev/pixie/src/shared/cvmsgs"
	"px.dev/pixie/src/shared/cvmsgspb"
)

// The table where these metrics are written.
const bqMetricsTable = "vzmetrics"

// Row represents a bq row.
type Row struct {
	Metric string `bigquery:"metric"`
	// Labels is a JSON encoded representation of the various labels.
	Labels    string    `bigquery:"labels"`
	Value     float64   `bigquery:"value"`
	Timestamp time.Time `bigquery:"timestamp"`
}

// Server defines an metrics server type.
type Server struct {
	nc        *nats.Conn
	bqDataset *bigquery.Dataset
	schema    bigquery.Schema

	done chan struct{}
	once sync.Once
}

// NewServer creates a server.
func NewServer(nc *nats.Conn, bqDataset *bigquery.Dataset) *Server {
	return &Server{
		nc:        nc,
		bqDataset: bqDataset,

		done: make(chan struct{}),
	}
}

// Start sets up the listeners starts handling messages.
func (s *Server) Start() {
	schema, err := bigquery.InferSchema(Row{})
	if err != nil {
		log.WithError(err).Fatal("Failed to infer row schema")
	}
	s.schema = schema

	table, err := s.createOrGetBQTable()
	if err != nil {
		log.WithError(err).Fatal("Failed to get table from BigQuery")
	}

	t := &bq.Table{
		Schema: schema,
		Table:  table,
	}

	for _, shard := range vzshard.GenerateShardRange() {
		bqWriteChan := make(chan interface{}, 4096)
		bqBatchInserter := bq.BatchInserter{
			BatchSize:   1024,
			PushTimeout: 5 * time.Minute,
			Table:       t,
		}

		s.startShardedHandler(shard, bqWriteChan)
		go bqBatchInserter.Run(bqWriteChan)
	}
}

func (s *Server) createOrGetBQTable() (*bigquery.Table, error) {
	table := s.bqDataset.Table(bqMetricsTable)

	// Check if the table already exists, if so, just return.
	_, err := table.Metadata(context.Background())
	if err == nil {
		return table, nil
	}

	// Table needs to be created.
	err = table.Create(context.Background(), &bigquery.TableMetadata{
		Schema: s.schema,
		TimePartitioning: &bigquery.TimePartitioning{
			Type:  bigquery.DayPartitioningType,
			Field: "timestamp",
		},
	})
	if err != nil {
		return nil, err
	}
	return table, nil
}

func (s *Server) startShardedHandler(shard string, bqWriteChan chan<- interface{}) {
	if s.nc == nil {
		return
	}
	natsCh := make(chan *nats.Msg, 8192)
	sub, err := s.nc.ChanSubscribe(fmt.Sprintf("v2c.%s.*.%s", shard, cvmsgs.VizierMetricsChannel), natsCh)
	if err != nil {
		log.WithError(err).Fatal("Failed to subscribe to NATS channel")
	}

	go func() {
		for {
			select {
			case <-s.done:
				sub.Unsubscribe()
				return
			case msg := <-natsCh:
				pb := &cvmsgspb.V2CMessage{}
				err := proto.Unmarshal(msg.Data, pb)
				if err != nil {
					log.WithError(err).Error("Could not unmarshal message")
					continue
				}
				anyMsg := pb.Msg
				wr := &prompb.WriteRequest{}
				err = types.UnmarshalAny(anyMsg, wr)
				if err != nil {
					log.WithError(err).Error("Could not nested message")
					continue
				}
				for _, ts := range wr.Timeseries {
					tsToRows(ts, s.schema, bqWriteChan)
				}
			}
		}
	}()
}

func tsToRows(timeseries prompb.TimeSeries, schema bigquery.Schema, bqWriteChan chan<- interface{}) {
	var metricName string
	labels := make(map[string]string)
	for _, l := range timeseries.Labels {
		if l.Name == model.MetricNameLabel {
			metricName = l.Value
			continue
		}
		labels[l.Name] = l.Value
	}
	labelsJSON, _ := json.Marshal(labels)

	for _, s := range timeseries.Samples {
		v := float64(s.Value)
		if math.IsNaN(v) || math.IsInf(v, 0) {
			continue
		}

		r := Row{
			Metric:    metricName,
			Labels:    string(labelsJSON),
			Value:     v,
			Timestamp: timestamp.Time(s.Timestamp),
		}

		bqWriteChan <- r
	}
}

// Stop performs any necessary cleanup before shutdown.
func (s *Server) Stop() {
	s.once.Do(func() {
		close(s.done)
	})
}
