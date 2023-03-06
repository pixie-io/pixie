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

	"px.dev/pixie/src/api/go/pxapi/types"
	"px.dev/pixie/src/e2e_test/perf_tool/experimentpb"
)

type singleMetricHandler struct {
	resultCh chan<- *ResultRow
	spec     *experimentpb.SingleMetricPxLOutput
}

// HandleInit implements the pxapi.TableRecordHandler interface.
func (h *singleMetricHandler) HandleInit(ctx context.Context, metadata types.TableMetadata) error {
	return nil
}

// HandleRecord implements the pxapi.TableRecordHandler interface.
func (h *singleMetricHandler) HandleRecord(ctx context.Context, r *types.Record) error {
	row := newResultRow()
	ts, err := getColumnAsTime(r, h.spec.TimestampCol, "SingleMetricPxLOutput.TimestampCol")
	if err != nil {
		return err
	}
	row.Timestamp = ts

	val, err := getColumnAsFloat(r, h.spec.ValueCol, "SingleMetricPxLOutput.ValueCol")
	if err != nil {
		return err
	}
	row.Value = val

	row.Name = h.spec.MetricName

	for i, tagCol := range h.spec.TagCols {
		tagVal, err := getColumnAsString(r, tagCol, fmt.Sprintf("SingleMetricPxLOutput.TagCols[%d]", i))
		if err != nil {
			return err
		}
		row.Tags[tagCol] = tagVal
	}

	select {
	case <-ctx.Done():
	case h.resultCh <- row:
	}
	return nil
}

// HandleDone implements the pxapi.TableRecordHandler interface.
func (h *singleMetricHandler) HandleDone(ctx context.Context) error {
	return nil
}
