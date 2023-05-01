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

package pxapi

import (
	"context"
	"errors"
	"fmt"
	"io"
	"strings"
	"sync"
	"time"

	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/api/go/pxapi/errdefs"
	"px.dev/pixie/src/api/go/pxapi/types"
	"px.dev/pixie/src/api/go/pxapi/utils"
	"px.dev/pixie/src/api/proto/vizierpb"
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
	c      vizierpb.VizierService_ExecuteScriptClient
	cancel context.CancelFunc
	closed bool

	tableIDToTracker map[string]*tableTracker
	tm               TableMuxer
	decOpts          *vizierpb.ExecuteScriptRequest_EncryptionOptions
	wg               sync.WaitGroup

	stats *ResultsStats

	v       *VizierClient
	queryID string
	origCtx context.Context
}

func newScriptResults() *ScriptResults {
	return &ScriptResults{
		tableIDToTracker: make(map[string]*tableTracker),
		stats:            &ResultsStats{},
	}
}

// Close will terminate the call.
func (s *ScriptResults) Close() error {
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

func (s *ScriptResults) handleGRPCMsg(ctx context.Context, resp *vizierpb.ExecuteScriptResponse) error {
	if err := errdefs.ParseStatus(resp.Status); err != nil {
		return err
	}
	switch v := resp.Result.(type) {
	case *vizierpb.ExecuteScriptResponse_MetaData:
		return s.handleTableMetadata(ctx, v)
	case *vizierpb.ExecuteScriptResponse_Data:
		if v.Data != nil {
			if v.Data.EncryptedBatch != nil {
				return s.handleEncryptedTableRowBatch(ctx, v.Data.EncryptedBatch)
			}
			if v.Data.Batch != nil {
				return s.handleTableRowbatch(ctx, v.Data.Batch)
			}
			if v.Data.ExecutionStats != nil {
				return s.handleStats(ctx, v.Data.ExecutionStats)
			}
		}
	}

	return errdefs.ErrInternalUnImplementedType
}

func isTransientGRPCError(err error) bool {
	s, ok := status.FromError(err)
	if !ok {
		return false
	}
	if s.Code() == codes.Internal && strings.Contains(s.Message(), "RST_STREAM") {
		return true
	}
	if s.Code() == codes.Internal && strings.Contains(s.Message(), "server closed the stream without sending trailers") {
		return true
	}
	return false
}

func (s *ScriptResults) reconnect() error {
	if s.queryID == "" {
		return errors.New("cannot reconnect to query that hasn't returned a QueryID yet")
	}
	req := &vizierpb.ExecuteScriptRequest{
		ClusterID:         s.v.vizierID,
		QueryID:           s.queryID,
		EncryptionOptions: s.v.encOpts,
	}
	ctx, cancel := context.WithCancel(s.origCtx)
	res, err := s.v.vzClient.ExecuteScript(s.v.cloud.cloudCtxWithMD(ctx), req)
	if err != nil {
		cancel()
		return err
	}
	s.cancel = cancel
	s.c = res
	return nil
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
			if isTransientGRPCError(err) {
				origErr := err
				err = s.reconnect()
				if err != nil {
					return fmt.Errorf("streaming failed: %w, error occurred while reconnecting: %v", origErr, err)
				}
				ctx = s.c.Context()
				continue
			}
			return err
		}
		if resp == nil {
			return nil
		}
		if s.queryID == "" {
			s.queryID = resp.QueryID
		}
		if err := s.handleGRPCMsg(ctx, resp); err != nil {
			return err
		}
	}
}

func (s *ScriptResults) handleTableMetadata(ctx context.Context, md *vizierpb.ExecuteScriptResponse_MetaData) error {
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
			err = handler.HandleInit(ctx, tableMD)
			if err != nil {
				return err
			}
		}
	}

	s.tableIDToTracker[qmd.ID] = &tableTracker{
		md:      tableMD,
		handler: handler,
		done:    false,
	}
	return nil
}

func (s *ScriptResults) handleEncryptedTableRowBatch(ctx context.Context, eb []byte) error {
	if s.decOpts == nil {
		return errdefs.ErrMissingDecryptionKey
	}
	batch, err := utils.DecodeRowBatch(s.decOpts, eb)
	if err != nil {
		return err
	}
	return s.handleTableRowbatch(ctx, batch)
}

func (s *ScriptResults) handleTableRowbatch(ctx context.Context, b *vizierpb.RowBatchData) error {
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

	if tracker.done {
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
		case vizierpb.BOOLEAN:
			row[colIdx] = types.NewBooleanValue(&colSchema)
		case vizierpb.INT64:
			row[colIdx] = types.NewInt64Value(&colSchema)
		case vizierpb.TIME64NS:
			row[colIdx] = types.NewTime64NSValue(&colSchema)
		case vizierpb.FLOAT64:
			row[colIdx] = types.NewFloat64Value(&colSchema)
		case vizierpb.STRING:
			row[colIdx] = types.NewStringValue(&colSchema)
		case vizierpb.UINT128:
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

func extractDataFromCol(colData []*vizierpb.Column, rowIdx, colIdx int64, row []types.Datum) error {
	switch colTyped := colData[colIdx].ColData.(type) {
	case *vizierpb.Column_BooleanData:
		dCasted, ok := row[colIdx].(*types.BooleanValue)
		if !ok {
			return errdefs.ErrInternalMismatchedType
		}
		dCasted.ScanBool(colTyped.BooleanData.Data[rowIdx])
	case *vizierpb.Column_Int64Data:
		dCasted, ok := row[colIdx].(*types.Int64Value)
		if !ok {
			return errdefs.ErrInternalMismatchedType
		}
		dCasted.ScanInt64(colTyped.Int64Data.Data[rowIdx])
	case *vizierpb.Column_Time64NsData:
		dCasted, ok := row[colIdx].(*types.Time64NSValue)
		if !ok {
			return errdefs.ErrInternalMismatchedType
		}
		dCasted.ScanInt64(colTyped.Time64NsData.Data[rowIdx])
	case *vizierpb.Column_Float64Data:
		dCasted, ok := row[colIdx].(*types.Float64Value)
		if !ok {
			return errdefs.ErrInternalMismatchedType
		}
		dCasted.ScanFloat64(colTyped.Float64Data.Data[rowIdx])
	case *vizierpb.Column_StringData:
		dCasted, ok := row[colIdx].(*types.StringValue)
		if !ok {
			return errdefs.ErrInternalMismatchedType
		}
		dCasted.ScanString(string(colTyped.StringData.Data[rowIdx]))
	case *vizierpb.Column_Uint128Data:
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

func (s *ScriptResults) handleStats(ctx context.Context, qes *vizierpb.QueryExecutionStats) error {
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
