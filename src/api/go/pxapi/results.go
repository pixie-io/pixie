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
 */

package pxapi

import (
	"context"
	"fmt"
	"io"
	"sync"
	"time"

	"go.withpixie.dev/pixie/src/api/go/pxapi/errdefs"
	"go.withpixie.dev/pixie/src/api/go/pxapi/types"
	vizierapipb "go.withpixie.dev/pixie/src/api/public/vizierapipb"
)

type tableTracker struct {
	md      types.TableMetadata
	handler TableRecordHandler
	done    bool
}

// ResultsStats stores statistics about the data.
type ResultsStats struct {
	AcceptedBytes int64
	TotalBytes    int64

	ExecutionTime    time.Duration
	CompilationTime  time.Duration
	BytesProcessed   int64
	RecordsProcessed int64
}

// ScriptResults tracks the results of a script, and provides mechanisms to cancel, etc.
type ScriptResults struct {
	c      vizierapipb.VizierService_ExecuteScriptClient
	cancel context.CancelFunc
	closed bool

	tableIDToTracker map[string]*tableTracker
	tm               TableMuxer
	wg               sync.WaitGroup

	stats *ResultsStats
}

func newScriptResults() *ScriptResults {
	return &ScriptResults{
		tableIDToTracker: make(map[string]*tableTracker),
		stats:            &ResultsStats{},
	}
}

// Close will terminate the call.
func (s ScriptResults) Close() error {
	// Cancel stream if still active.
	select {
	case <-s.c.Context().Done():
	default:
		s.cancel()
	}

	// Wait for stream routine to end if it's still running.
	s.wg.Wait()
	s.closed = true

	return nil
}

// Stream will start streaming the results. Since the API is streaming fist, even a non streaming
// script requires this to be called to get all the results.
func (s *ScriptResults) Stream() error {
	if s.closed {
		return errdefs.ErrStreamAlreadyClosed
	}

	// Check if the context has already terminated.
	select {
	case <-s.c.Context().Done():
		return errdefs.ErrStreamAlreadyClosed
	default:
	}

	s.wg.Add(1)
	var streamErr error
	go func() {
		defer s.wg.Done()
		streamErr = s.run()
	}()

	s.wg.Wait()
	return streamErr
}

func (s *ScriptResults) run() error {
	ctx := s.c.Context()
	for {
		resp, err := s.c.Recv()

		if err != nil {
			if err == io.EOF {
				// Stream has terminated.
				return nil
			}
			return err
		}
		if resp == nil {
			return nil
		}

		switch v := resp.Result.(type) {
		case *vizierapipb.ExecuteScriptResponse_MetaData:
			err = s.handleTableMetadata(ctx, v)
		case *vizierapipb.ExecuteScriptResponse_Data:
			if v.Data != nil {
				if v.Data.Batch != nil {
					err = s.handleTableRowbatch(ctx, v.Data.Batch)
				}
				if v.Data.ExecutionStats != nil {
					err = s.handleStats(ctx, v.Data.ExecutionStats)
				}
			}
		}
		if err != nil {
			return err
		}
	}
}

func (s *ScriptResults) handleTableMetadata(ctx context.Context, md *vizierapipb.ExecuteScriptResponse_MetaData) error {
	qmd := md.MetaData

	// New table, check and see if we are already tracking it.
	if _, has := s.tableIDToTracker[qmd.ID]; has {
		return errdefs.ErrInternalDuplicateTableMetadata
	}

	colInfo := make([]types.ColSchema, len(qmd.Relation.Columns))
	colIdxByName := make(map[string]int64)
	for idx, col := range qmd.Relation.Columns {
		colInfo[idx] = types.ColSchema{
			Name:         col.ColumnName,
			Type:         col.ColumnType,
			SemanticType: col.ColumnSemanticType,
		}
		colIdxByName[col.ColumnName] = int64(idx)
	}

	// Create the table schema.
	tableMD := types.TableMetadata{
		Name:         qmd.Name,
		ColInfo:      colInfo,
		ColIdxByName: colIdxByName,
	}

	// Check to see where to route the table, or if it should be dropped.
	var handler TableRecordHandler
	var err error
	if s.tm != nil {
		handler, err = s.tm.AcceptTable(ctx, tableMD)
		if err != nil {
			return err
		}
		if handler != nil {
			handler.HandleInit(ctx, tableMD)
		}
	}

	s.tableIDToTracker[qmd.ID] = &tableTracker{
		md:      tableMD,
		handler: handler,
		done:    false,
	}
	return nil
}

func (s *ScriptResults) handleTableRowbatch(ctx context.Context, b *vizierapipb.RowBatchData) error {
	tracker, ok := s.tableIDToTracker[b.TableID]
	if !ok {
		return errdefs.ErrInternalMissingTableMetadata
	}
	s.stats.AcceptedBytes += int64(b.Size())
	if tracker.handler == nil {
		// No handler specified for this table, skip it.
		return nil
	}
	s.stats.TotalBytes += int64(b.Size())

	if tracker.done == true {
		return errdefs.ErrInternalDataAfterEOS
	}

	handler := tracker.handler
	// Loop through records, do the type conversion and call HandleRecord for each row of data.
	numCols := int64(len(b.Cols))
	row := make([]types.Datum, numCols)
	record := &types.Record{
		Data:          row,
		TableMetadata: &tracker.md,
	}

	// Go through every column and produce the correct value type for each column.
	for colIdx := int64(0); colIdx < numCols; colIdx++ {
		colSchema := tracker.md.ColInfo[colIdx]
		// This code is a bit ugly because the API ships the data in types columns.
		// So we lookup the data type and then create a row pointer that corresponds to the correct
		// value type.
		switch tracker.md.ColInfo[colIdx].Type {
		case vizierapipb.BOOLEAN:
			row[colIdx] = types.NewBooleanValue(&colSchema)
		case vizierapipb.INT64:
			row[colIdx] = types.NewInt64Value(&colSchema)
		case vizierapipb.TIME64NS:
			row[colIdx] = types.NewTime64NSValue(&colSchema)
		case vizierapipb.FLOAT64:
			row[colIdx] = types.NewFloat64Value(&colSchema)
		case vizierapipb.STRING:
			row[colIdx] = types.NewStringValue(&colSchema)
		case vizierapipb.UINT128:
			row[colIdx] = types.NewUint128Value(&colSchema)
		}
	}

	// Now loop through the rows, covert the values at each column and call the handler.
	for rowIdx := int64(0); rowIdx < b.NumRows; rowIdx++ {
		for colIdx := int64(0); colIdx < numCols; colIdx++ {
			if err := extractDataFromCol(b.Cols, rowIdx, colIdx, row); err != nil {
				return err
			}
		}
		if err := handler.HandleRecord(ctx, record); err != nil {
			return err
		}
	}

	// This table has been completely streamed.
	if b.Eos {
		tracker.done = true
		return handler.HandleDone(ctx)
	}
	return nil
}

func extractDataFromCol(colData []*vizierapipb.Column, rowIdx, colIdx int64, row []types.Datum) error {
	switch colTyped := colData[colIdx].ColData.(type) {
	case *vizierapipb.Column_BooleanData:
		dCasted, ok := row[colIdx].(*types.BooleanValue)
		if !ok {
			return errdefs.ErrInternalMismatchedType
		}
		dCasted.ScanBool(colTyped.BooleanData.Data[rowIdx])
	case *vizierapipb.Column_Int64Data:
		dCasted, ok := row[colIdx].(*types.Int64Value)
		if !ok {
			return errdefs.ErrInternalMismatchedType
		}
		dCasted.ScanInt64(colTyped.Int64Data.Data[rowIdx])
	case *vizierapipb.Column_Time64NsData:
		dCasted, ok := row[colIdx].(*types.Time64NSValue)
		if !ok {
			fmt.Printf("Here %+v", row[colIdx].Type())
			return errdefs.ErrInternalMismatchedType
		}
		dCasted.ScanInt64(colTyped.Time64NsData.Data[rowIdx])
	case *vizierapipb.Column_Float64Data:
		dCasted, ok := row[colIdx].(*types.Float64Value)
		if !ok {
			return errdefs.ErrInternalMismatchedType
		}
		dCasted.ScanFloat64(colTyped.Float64Data.Data[rowIdx])
	case *vizierapipb.Column_StringData:
		dCasted, ok := row[colIdx].(*types.StringValue)
		if !ok {
			return errdefs.ErrInternalMismatchedType
		}
		dCasted.ScanString(colTyped.StringData.Data[rowIdx])
	case *vizierapipb.Column_Uint128Data:
		dCasted, ok := row[colIdx].(*types.UInt128Value)
		if !ok {
			return errdefs.ErrInternalMismatchedType
		}
		dCasted.ScanUInt128(colTyped.Uint128Data.Data[rowIdx])
	default:
		return errdefs.ErrInternalUnImplementedType
	}
	return nil
}

func (s *ScriptResults) handleStats(ctx context.Context, qes *vizierapipb.QueryExecutionStats) error {
	s.stats.BytesProcessed += qes.BytesProcessed
	s.stats.RecordsProcessed += qes.RecordsProcessed
	s.stats.CompilationTime = time.Duration(qes.Timing.CompilationTimeNs) * time.Nanosecond
	s.stats.ExecutionTime = time.Duration(qes.Timing.ExecutionTimeNs) * time.Nanosecond
	return nil
}

// Stats returns the execution and script stats.
func (s *ScriptResults) Stats() *ResultsStats {
	return s.stats
}
