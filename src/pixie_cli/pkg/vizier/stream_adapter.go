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

package vizier

import (
	"context"
	"encoding/binary"
	"errors"
	"fmt"
	"io"
	"os"
	"reflect"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/gofrs/uuid"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc/status"

	apiutils "px.dev/pixie/src/api/go/pxapi/utils"
	"px.dev/pixie/src/api/proto/vizierpb"
	"px.dev/pixie/src/pixie_cli/pkg/components"
	"px.dev/pixie/src/pixie_cli/pkg/utils"
)

// StreamWriterFactorFunc is a stream writer factory.
type StreamWriterFactorFunc = func(md *vizierpb.ExecuteScriptResponse_MetaData) components.OutputStreamWriter

// TableInfo contains the information about a table.
type TableInfo struct {
	w          components.OutputStreamWriter
	ID         string
	relation   *vizierpb.Relation
	timeColIdx int
}

// ExecData contains information from script executions.
type ExecData struct {
	Resp      *vizierpb.ExecuteScriptResponse
	ClusterID uuid.UUID
	Err       error
}

// StreamOutputAdapter adapts the vizier output to the StreamWriters.
type StreamOutputAdapter struct {
	tableNameToInfo     map[string]*TableInfo
	execStats           *vizierpb.QueryExecutionStats
	streamWriterFactory StreamWriterFactorFunc
	wg                  sync.WaitGroup
	enableFormat        bool
	format              string
	formatters          map[string]DataFormatter
	mutationInfo        *vizierpb.MutationInfo
	decOpts             *vizierpb.ExecuteScriptRequest_EncryptionOptions

	// This is used to track table/ID -> names across multiple clusters.
	tabledIDToName map[string]string

	// Captures error if any on the stream and returns it with Finish.
	err error

	totalBytes int
}

var (
	// ErrMetadataMissing is returned when table was malformed missing data.
	ErrMetadataMissing = errors.New("metadata missing for table")
	// ErrDuplicateMetadata is returned when table is malformed an contains multiple metadata.
	ErrDuplicateMetadata = errors.New("duplicate table metadata received")
)

// FormatInMemory denotes the inmemory format.
const FormatInMemory string = "inmemory"

// NewStreamOutputAdapterWithFactory creates a new vizier output adapter factory.
func NewStreamOutputAdapterWithFactory(ctx context.Context, stream chan *ExecData, format string,
	decOpts *vizierpb.ExecuteScriptRequest_EncryptionOptions,
	factoryFunc func(*vizierpb.ExecuteScriptResponse_MetaData) components.OutputStreamWriter) *StreamOutputAdapter {
	enableFormat := format != "json" && format != FormatInMemory

	adapter := &StreamOutputAdapter{
		tableNameToInfo:     make(map[string]*TableInfo),
		streamWriterFactory: factoryFunc,
		format:              format,
		enableFormat:        enableFormat,
		formatters:          make(map[string]DataFormatter),
		tabledIDToName:      make(map[string]string),
		decOpts:             decOpts,
	}

	adapter.wg.Add(1)
	go adapter.handleStream(ctx, stream)

	return adapter
}

// NewStreamOutputAdapter creates a new vizier output adapter.
func NewStreamOutputAdapter(ctx context.Context, stream chan *ExecData, format string, decOpts *vizierpb.ExecuteScriptRequest_EncryptionOptions) *StreamOutputAdapter {
	factoryFunc := func(md *vizierpb.ExecuteScriptResponse_MetaData) components.OutputStreamWriter {
		return components.CreateStreamWriter(format, os.Stdout)
	}
	return NewStreamOutputAdapterWithFactory(ctx, stream, format, decOpts, factoryFunc)
}

// Finish must be called to wait for the output and flush all the data.
func (v *StreamOutputAdapter) Finish() error {
	v.wg.Wait()

	if v.err != nil {
		return v.err
	}

	for _, ti := range v.tableNameToInfo {
		ti.w.Finish()
	}
	return nil
}

// WaitForCompletion waits for the stream to complete, but does not flush the data.
func (v *StreamOutputAdapter) WaitForCompletion() error {
	v.wg.Wait()
	if v.err != nil {
		return v.err
	}
	return nil
}

// ExecStats returns the reported execution stats. This function is only valid with format = inmemory and after Finish.
func (v *StreamOutputAdapter) ExecStats() (*vizierpb.QueryExecutionStats, error) {
	if v.execStats == nil {
		return nil, fmt.Errorf("ExecStats not found")
	}
	return v.execStats, nil
}

// MutationInfo returns the mutation info. This function is only valid after Finish.
func (v *StreamOutputAdapter) MutationInfo() (*vizierpb.MutationInfo, error) {
	if v.mutationInfo == nil {
		return nil, fmt.Errorf("MutationInfo not found")
	}
	return v.mutationInfo, nil
}

// Views gets all the accumulated views. This function is only valid with format = inmemory and after Finish.
func (v *StreamOutputAdapter) Views() ([]components.TableView, error) {
	if v.err != nil {
		return nil, v.err
	}
	// This function only works for in memory format.
	if v.format != FormatInMemory {
		return nil, errors.New("invalid format")
	}
	views := make([]components.TableView, 0)
	for _, ti := range v.tableNameToInfo {
		var ok bool
		vitv, ok := ti.w.(components.TableView)
		if !ok {
			return nil, errors.New("cannot convert to table view")
		}
		views = append(views, vitv)
	}
	return views, nil
}

// Formatters gets all the data formatters. This function is only valid with format = inmemory and after Finish.
func (v *StreamOutputAdapter) Formatters() ([]DataFormatter, error) {
	if v.err != nil {
		return nil, v.err
	}
	// This function only works for in memory format.
	if v.format != FormatInMemory {
		return nil, errors.New("invalid format")
	}
	formatters := make([]DataFormatter, 0)
	for _, ti := range v.tableNameToInfo {
		formatters = append(formatters, NewDataFormatterForTable(ti.relation))
	}
	return formatters, nil
}

func (v *StreamOutputAdapter) handleStream(ctx context.Context, stream chan *ExecData) {
	defer v.wg.Done()
	for {
		select {
		case <-ctx.Done():
			if err := ctx.Err(); err != nil {
				if errors.Is(err, context.Canceled) {
					v.err = newScriptExecutionError(CodeCanceled, err.Error())
					return
				}
				if errors.Is(err, context.DeadlineExceeded) {
					v.err = newScriptExecutionError(CodeTimeout, err.Error())
					return
				}
				v.err = newScriptExecutionError(CodeUnknown, err.Error())
			}
			return
		case msg := <-stream:
			if msg == nil {
				return
			}
			if msg.Err != nil {
				if msg.Err == io.EOF {
					return
				}
				grpcErr, ok := status.FromError(msg.Err)
				if ok {
					v.err = newScriptExecutionError(CodeGRPCError, "Failed to execute script: "+grpcErr.Message())
					return
				}
				v.err = newScriptExecutionError(CodeUnknown, "failed to execute script")
				return
			}

			if msg.Resp.Status != nil && msg.Resp.Status.Code != 0 {
				// Try to parse the error and return it up stream.
				v.err = v.parseError(ctx, msg.Resp.Status)
				return
			}

			if msg.Resp.MutationInfo != nil {
				v.handleMutationInfo(ctx, msg.Resp.MutationInfo)
				continue
			}

			if msg.Resp.Result == nil {
				v.err = newScriptExecutionError(CodeUnknown, "Got empty response")
				return
			}

			v.totalBytes += msg.Resp.Size()
			var err error
			switch res := msg.Resp.Result.(type) {
			case *vizierpb.ExecuteScriptResponse_MetaData:
				err = v.handleMetadata(ctx, res)
			case *vizierpb.ExecuteScriptResponse_Data:
				err = v.handleData(ctx, res)
			default:
				err = fmt.Errorf("unhandled response type" + reflect.TypeOf(msg.Resp.Result).String())
			}
			if err != nil {
				v.err = newScriptExecutionError(CodeBadData, "failed to handle data from Vizier: "+err.Error())
				return
			}
		}
	}
}

// TotalBytes returns the total bytes of messages passed to this adapter.
func (v *StreamOutputAdapter) TotalBytes() int {
	return v.totalBytes
}

// getNumRows returns the number of rows in the input column.
func getNumRows(in *vizierpb.Column) int {
	switch u := in.ColData.(type) {
	case *vizierpb.Column_StringData:
		return len(u.StringData.Data)
	case *vizierpb.Column_Float64Data:
		return len(u.Float64Data.Data)
	case *vizierpb.Column_Int64Data:
		return len(u.Int64Data.Data)
	case *vizierpb.Column_Time64NsData:
		return len(u.Time64NsData.Data)
	case *vizierpb.Column_BooleanData:
		return len(u.BooleanData.Data)
	case *vizierpb.Column_Uint128Data:
		return len(u.Uint128Data.Data)
	}
	return 0
}

// getNativeTypedValue returns the plucked data as a Go not vizierpb type.
func (v *StreamOutputAdapter) getNativeTypedValue(tableInfo *TableInfo, rowIdx int, colIdx int, data interface{}) interface{} {
	switch u := data.(type) {
	case *vizierpb.Column_StringData:
		s := string(u.StringData.Data[rowIdx])
		if f, err := strconv.ParseFloat(s, 64); err == nil {
			return f
		}
		if i, err := strconv.ParseInt(s, 10, 64); err == nil {
			return i
		}
		return string(u.StringData.Data[rowIdx])
	case *vizierpb.Column_Float64Data:
		return u.Float64Data.Data[rowIdx]
	case *vizierpb.Column_Int64Data:
		// TODO(zasgar): We really should not need this, but some of our operations convert time
		// to int64. We need to maintain types in the engine/compiler so that proper type casting can be done.
		if colIdx == tableInfo.timeColIdx {
			return time.Unix(0, u.Int64Data.Data[rowIdx])
		}
		return u.Int64Data.Data[rowIdx]
	case *vizierpb.Column_Time64NsData:
		return time.Unix(0, u.Time64NsData.Data[rowIdx])
	case *vizierpb.Column_BooleanData:
		return u.BooleanData.Data[rowIdx]
	case *vizierpb.Column_Uint128Data:
		b := make([]byte, 16)
		b2 := b[8:]
		binary.BigEndian.PutUint64(b, u.Uint128Data.Data[rowIdx].High)
		binary.BigEndian.PutUint64(b2, u.Uint128Data.Data[rowIdx].Low)
		return uuid.FromBytesOrNil(b)
	default:
		log.WithField("value", u).Fatalln("unknown data type")
	}
	return nil
}

func (v *StreamOutputAdapter) parseError(ctx context.Context, s *vizierpb.Status) error {
	var compilerErrors []string
	if s.ErrorDetails != nil {
		for _, ed := range s.ErrorDetails {
			if e, ok := ed.Error.(*vizierpb.ErrorDetails_CompilerError); ok {
				compilerErrors = append(compilerErrors,
					fmt.Sprintf("L%d : C%d  %s\n",
						e.CompilerError.Line, e.CompilerError.Column,
						e.CompilerError.Message))
			}
		}
	}

	if len(compilerErrors) > 0 {
		err := newScriptExecutionError(CodeCompilerError,
			fmt.Sprintf("Script compilation failed: %s", strings.Join(compilerErrors, ", ")))
		err.compilerErrors = compilerErrors
		return err
	}

	utils.Errorf("Script execution error: %s", s.Message)
	return newScriptExecutionError(CodeUnknown, "Script execution error:"+s.Message)
}

func (v *StreamOutputAdapter) handleExecutionStats(ctx context.Context, es *vizierpb.QueryExecutionStats) error {
	v.execStats = es
	return nil
}

func (v *StreamOutputAdapter) handleMutationInfo(ctx context.Context, mi *vizierpb.MutationInfo) {
	v.mutationInfo = mi
}

func (v *StreamOutputAdapter) handleData(ctx context.Context, d *vizierpb.ExecuteScriptResponse_Data) error {
	if d.Data.ExecutionStats != nil {
		err := v.handleExecutionStats(ctx, d.Data.ExecutionStats)
		if err != nil {
			return err
		}
	}

	if v.decOpts != nil {
		if d.Data.Batch != nil {
			log.Warn("Expected script results to be encrypted, please upgrade vizier.")
		} else if d.Data.EncryptedBatch != nil {
			batch, err := apiutils.DecodeRowBatch(v.decOpts, d.Data.EncryptedBatch)
			if err != nil {
				return err
			}
			d.Data.Batch = batch
			d.Data.EncryptedBatch = nil
		}
	}

	if d.Data.Batch == nil {
		return nil
	}
	tableName := v.tabledIDToName[d.Data.Batch.TableID]
	tableInfo, ok := v.tableNameToInfo[tableName]
	if !ok {
		return ErrMetadataMissing
	}
	formatter, ok := v.formatters[tableName]
	if !ok {
		return ErrMetadataMissing
	}

	var numRows int
	if d.Data != nil && d.Data.Batch != nil && d.Data.Batch.Cols != nil {
		numRows = getNumRows(d.Data.Batch.Cols[0])
	} else {
		// No records.
		return nil
	}

	cols := d.Data.Batch.Cols
	for rowIdx := 0; rowIdx < numRows; rowIdx++ {
		// Add the cluster ID to the output colums.
		rec := make([]interface{}, len(cols))
		for colIdx, col := range cols {
			val := v.getNativeTypedValue(tableInfo, rowIdx, colIdx, col.ColData)
			if v.enableFormat {
				rec[colIdx] = formatter.FormatValue(colIdx, val)
			} else {
				rec[colIdx] = val
			}
		}
		ti := v.tableNameToInfo[tableName]
		if err := ti.w.Write(rec); err != nil {
			return err
		}
	}
	return nil
}

func (v *StreamOutputAdapter) handleMetadata(ctx context.Context, md *vizierpb.ExecuteScriptResponse_MetaData) error {
	tableName := md.MetaData.Name
	newWriter := v.streamWriterFactory(md)

	if _, exists := v.tabledIDToName[md.MetaData.ID]; exists {
		return ErrDuplicateMetadata
	}

	v.tabledIDToName[md.MetaData.ID] = md.MetaData.Name
	if _, exists := v.tableNameToInfo[tableName]; exists {
		// We already have metadata for this table.
		// TODO(zasgar): Add more strict check to make sure all this MD is consistent
		// across multiple viziers.
		return nil
	}
	relation := md.MetaData.Relation

	timeColIdx := -1
	for idx, col := range relation.Columns {
		if col.ColumnName == "time_" {
			timeColIdx = idx
			break
		}
	}

	// Write out the header keys in the order specified by the relation.
	headerKeys := make([]string, len(relation.Columns))
	for i, col := range relation.Columns {
		headerKeys[i] = col.ColumnName
	}
	newWriter.SetHeader(md.MetaData.Name, headerKeys)

	v.tableNameToInfo[tableName] = &TableInfo{
		ID:         tableName,
		w:          newWriter,
		relation:   relation,
		timeColIdx: timeColIdx,
	}

	v.formatters[tableName] = NewDataFormatterForTable(relation)
	return nil
}
