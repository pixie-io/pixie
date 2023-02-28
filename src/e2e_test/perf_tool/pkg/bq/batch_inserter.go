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

package bq

import (
	"context"
	"time"

	"cloud.google.com/go/bigquery"
	log "github.com/sirupsen/logrus"
)

// BatchInserter batches up bigquery struct savers, to reduce the number of calls to inserter.Put.
type BatchInserter struct {
	// BatchSize specifies how many rows (struct savers) to wait for before pushing a batch.
	BatchSize int
	// PushTimeout specifies how long to wait before pushing a batch even if the batch isn't at BatchSize yet.
	PushTimeout time.Duration
	Table       *Table

	batch    []*bigquery.StructSaver
	inserter *bigquery.Inserter
}

// Run starts running the BatchInserter.
func (b *BatchInserter) Run(rowCh <-chan interface{}) {
	b.inserter = b.Table.Inserter()
	b.inserter.SkipInvalidRows = false

	b.batch = make([]*bigquery.StructSaver, 0)

	t := time.NewTicker(b.PushTimeout)

	channelClosed := false
	for !channelClosed {
		select {
		case <-t.C:
			b.uploadBatch()
		case row, ok := <-rowCh:
			if !ok {
				channelClosed = true
				break
			}
			if len(b.batch) >= b.BatchSize {
				b.uploadBatch()
			}
			b.addRow(row)
		}
	}
	b.uploadBatch()
}

func (b *BatchInserter) addRow(row interface{}) {
	s := &bigquery.StructSaver{
		Struct: row,
		Schema: b.Table.Schema,
	}
	b.batch = append(b.batch, s)
}

func (b *BatchInserter) uploadBatch() {
	if len(b.batch) == 0 {
		return
	}
	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Minute)
	defer cancel()
	err := b.inserter.Put(ctx, b.batch)
	if err != nil {
		log.WithError(err).Error("failed to insert batch into bigquery")
	}
	b.batch = b.batch[:0]
}
