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

package metrics

import (
	"context"
	"fmt"
	"time"

	prototypes "github.com/gogo/protobuf/types"

	"px.dev/pixie/src/api/go/pxapi/types"
	"px.dev/pixie/src/e2e_test/perf_tool/experimentpb"
)

type dataLossHandler struct {
	resultCh     chan<- *ResultRow
	spec         *experimentpb.DataLossCounterOutput
	t            *time.Ticker
	outputPeriod time.Duration

	seq           *seqTracker
	lastTimestamp time.Time
}

// HandleInit implements the pxapi.TableRecordHandler interface.
func (h *dataLossHandler) HandleInit(ctx context.Context, metadata types.TableMetadata) error {
	h.seq = newSeqTracker()
	dur, err := prototypes.DurationFromProto(h.spec.OutputPeriod)
	if err != nil {
		return err
	}
	h.outputPeriod = dur
	return nil
}

// HandleRecord implements the pxapi.TableRecordHandler interface.
func (h *dataLossHandler) HandleRecord(ctx context.Context, r *types.Record) error {
	if h.t == nil {
		h.t = time.NewTicker(h.outputPeriod)
	}
	seqID, err := getColumnAsInt64(r, h.spec.SeqIDCol, "DataLossCounterOutput.SeqIDCol")
	if err != nil {
		return err
	}
	h.seq.Update(seqID)

	ts, err := getColumnAsTime(r, h.spec.TimestampCol, "DataLossCounterOutput.TimestampCol")
	if err != nil {
		return err
	}
	h.lastTimestamp = ts

	select {
	case <-h.t.C:
		if err := h.sendRows(ctx); err != nil {
			return err
		}
	default:
	}
	return nil
}

// HandleDone implements the pxapi.TableRecordHandler interface.
func (h *dataLossHandler) HandleDone(ctx context.Context) error {
	return h.sendRows(ctx)
}

func (h *dataLossHandler) sendRows(ctx context.Context) error {
	ts := h.lastTimestamp
	if (ts == time.Time{}) {
		ts = time.Now()
	}
	percentRow := newResultRow()
	percentRow.Timestamp = ts
	percentRow.Name = h.spec.MetricName
	val := float64(1.0)
	if h.seq.skipped != 0 || h.seq.valid != 0 {
		val = float64(h.seq.skipped) / float64(h.seq.skipped+h.seq.valid)
	}
	percentRow.Value = val
	h.sendRow(ctx, percentRow)

	countRow := newResultRow()
	countRow.Timestamp = ts
	countRow.Name = fmt.Sprintf("%s_count", h.spec.MetricName)
	countRow.Value = float64(h.seq.valid + h.seq.skipped)
	h.sendRow(ctx, countRow)
	return nil
}

func (h *dataLossHandler) sendRow(ctx context.Context, row *ResultRow) {
	select {
	case <-ctx.Done():
	case h.resultCh <- row:
	}
}

type seqTracker struct {
	maxSeen int64
	valid   int64
	skipped int64
}

func newSeqTracker() *seqTracker {
	return &seqTracker{
		maxSeen: -1,
		valid:   0,
		skipped: 0,
	}
}

func (s *seqTracker) Update(seqID int64) {
	s.valid++
	// This algorithm assumes that seqIDs are never duplicated.
	if seqID > s.maxSeen {
		// If seqID is greater than the max seq ID we've seen, then all of the seqID in between maxSeen and seqID were skipped.
		// If seqID is maxSeen + 1, then we didn't skip any, and the calculation below will add 0 to skipped.
		s.skipped += (seqID - s.maxSeen) - 1
		s.maxSeen = seqID
	} else {
		// If seqID is less than maxSeen, then this record came in out of order, and was previously considered skipped.
		s.skipped--
	}
}
