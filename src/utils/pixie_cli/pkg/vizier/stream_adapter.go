package vizier

import (
	"context"
	"encoding/binary"
	"errors"
	"fmt"
	"io"
	"os"
	"reflect"
	"regexp"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/fatih/color"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc/status"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/components"
	cliLog "pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/utils"
	pl_api_vizierpb "pixielabs.ai/pixielabs/src/vizier/vizierpb"
)

type StreamWriterFactorFunc = func(md *pl_api_vizierpb.ExecuteScriptResponse_MetaData) components.OutputStreamWriter

type TableInfo struct {
	w           components.OutputStreamWriter
	ID          string
	relation    *pl_api_vizierpb.Relation
	timeColIdx  int
	latencyCols map[int]bool
	alertCols   map[int]bool
}

type VizierExecData struct {
	Resp      *pl_api_vizierpb.ExecuteScriptResponse
	ClusterID uuid.UUID
	Err       error
}

// VizierStreamOutputAdapter adapts the vizier output to the StreamWriters.
type VizierStreamOutputAdapter struct {
	tableNameToInfo     map[string]*TableInfo
	execStats           *pl_api_vizierpb.QueryExecutionStats
	streamWriterFactory StreamWriterFactorFunc
	wg                  sync.WaitGroup
	enableFormat        bool
	format              string
	mutationInfo        *pl_api_vizierpb.MutationInfo

	// This is used to track table/ID -> names across multiple clusters.
	tabledIDToName map[string]string

	// Captures error if any on the stream and returns it with Finish.
	err                      error
	latencyYellowThresholdMS float64
	latencyRedThresholdMS    float64

	// Internally used functions.
	redSprintf    func(format string, a ...interface{}) string
	yellowSprintf func(format string, a ...interface{}) string

	totalBytes int
}

var (
	// ErrMetadataMissing is returned when table was malformed missing data.
	ErrMetadataMissing = errors.New("metadata missing for table")
	// ErrDuplicateMetadata is returned when table is malformed an contains multiple metadata.
	ErrDuplicateMetadata = errors.New("duplicate table metadata received")
)

const FormatInMemory string = "inmemory"

// NewVizierStreamOutputAdapter creates a new vizier output adapter.
func NewVizierStreamOutputAdapterWithFactory(ctx context.Context, stream chan *VizierExecData, format string,
	factoryFunc func(*pl_api_vizierpb.ExecuteScriptResponse_MetaData) components.OutputStreamWriter) *VizierStreamOutputAdapter {
	enableFormat := format != "json" && format != FormatInMemory

	adapter := &VizierStreamOutputAdapter{
		tableNameToInfo:          make(map[string]*TableInfo),
		streamWriterFactory:      factoryFunc,
		format:                   format,
		enableFormat:             enableFormat,
		tabledIDToName:           make(map[string]string, 0),
		latencyYellowThresholdMS: 200,
		latencyRedThresholdMS:    400,
		redSprintf:               color.New(color.FgRed).SprintfFunc(),
		yellowSprintf:            color.New(color.FgYellow).SprintfFunc(),
	}

	adapter.wg.Add(1)
	go adapter.handleStream(ctx, stream)

	return adapter

}

// NewVizierStreamOutputAdapter creates a new vizier output adapter.
func NewVizierStreamOutputAdapter(ctx context.Context, stream chan *VizierExecData, format string) *VizierStreamOutputAdapter {
	factoryFunc := func(md *pl_api_vizierpb.ExecuteScriptResponse_MetaData) components.OutputStreamWriter {
		return components.CreateStreamWriter(format, os.Stdout)
	}
	return NewVizierStreamOutputAdapterWithFactory(ctx, stream, format, factoryFunc)
}

// Finish must be called to wait for the output and flush all the data.
func (v *VizierStreamOutputAdapter) Finish() error {
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
func (v *VizierStreamOutputAdapter) WaitForCompletion() error {
	v.wg.Wait()
	if v.err != nil {
		return v.err
	}
	return nil
}

// ExecStats returns the reported execution stats. This function is only valid with format = inmemory and after Finish.
func (v *VizierStreamOutputAdapter) ExecStats() (*pl_api_vizierpb.QueryExecutionStats, error) {
	if v.execStats == nil {
		return nil, fmt.Errorf("ExecStats not found")
	}
	return v.execStats, nil
}

// MutationInfo returns the mutation info. This function is only valid after Finish.
func (v *VizierStreamOutputAdapter) MutationInfo() (*pl_api_vizierpb.MutationInfo, error) {
	if v.mutationInfo == nil {
		return nil, fmt.Errorf("MutationInfo not found")
	}
	return v.mutationInfo, nil
}

// Views gets all the accumulated views. This function is only valid with format = inmemory and after Finish.
func (v *VizierStreamOutputAdapter) Views() ([]components.TableView, error) {
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

func (v *VizierStreamOutputAdapter) handleStream(ctx context.Context, stream chan *VizierExecData) {
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
			case *pl_api_vizierpb.ExecuteScriptResponse_MetaData:
				err = v.handleMetadata(ctx, res)
			case *pl_api_vizierpb.ExecuteScriptResponse_Data:
				err = v.handleData(ctx, msg.ClusterID.String(), res)
			default:
				panic("unhandled response type" + reflect.TypeOf(msg.Resp.Result).String())
			}
			if err != nil {
				v.err = newScriptExecutionError(CodeBadData, "failed to handle data from Vizier: "+err.Error())
				return
			}
		}
	}
}

// TotalBytes returns the total bytes of messages passed to this adapter.
func (v *VizierStreamOutputAdapter) TotalBytes() int {
	return v.totalBytes
}

// getNumRows returns the number of rows in the input column.
func getNumRows(in *pl_api_vizierpb.Column) int {
	switch u := in.ColData.(type) {
	case *pl_api_vizierpb.Column_StringData:
		return len(u.StringData.Data)
	case *pl_api_vizierpb.Column_Float64Data:
		return len(u.Float64Data.Data)
	case *pl_api_vizierpb.Column_Int64Data:
		return len(u.Int64Data.Data)
	case *pl_api_vizierpb.Column_Time64NsData:
		return len(u.Time64NsData.Data)
	case *pl_api_vizierpb.Column_BooleanData:
		return len(u.BooleanData.Data)
	case *pl_api_vizierpb.Column_Uint128Data:
		return len(u.Uint128Data.Data)
	}
	return 0
}

// getNativeTypedValue returns the plucked data as a Go not pl_api_vizierpb type.
func (v *VizierStreamOutputAdapter) getNativeTypedValue(tableInfo *TableInfo, rowIdx int, colIdx int, data interface{}) interface{} {
	switch u := data.(type) {
	case *pl_api_vizierpb.Column_StringData:
		s := u.StringData.Data[rowIdx]
		if f, err := strconv.ParseFloat(s, 64); err == nil {
			return f
		}
		if i, err := strconv.ParseInt(s, 10, 64); err == nil {
			return i
		}
		return u.StringData.Data[rowIdx]
	case *pl_api_vizierpb.Column_Float64Data:
		return u.Float64Data.Data[rowIdx]
	case *pl_api_vizierpb.Column_Int64Data:
		// TODO(zasgar): We really should not need this, but some of our operations convert time
		// to int64. We need to maintain types in the engine/compiler so that proper type casting can be done.
		if colIdx == tableInfo.timeColIdx {
			return time.Unix(0, u.Int64Data.Data[rowIdx])
		} else {
			return u.Int64Data.Data[rowIdx]
		}
	case *pl_api_vizierpb.Column_Time64NsData:
		return time.Unix(0, u.Time64NsData.Data[rowIdx])
	case *pl_api_vizierpb.Column_BooleanData:
		return u.BooleanData.Data[rowIdx]
	case *pl_api_vizierpb.Column_Uint128Data:
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
func (v *VizierStreamOutputAdapter) parseError(ctx context.Context, s *pl_api_vizierpb.Status) error {
	var compilerErrors []string
	if s.ErrorDetails != nil {
		for _, ed := range s.ErrorDetails {
			switch e := ed.Error.(type) {
			case *pl_api_vizierpb.ErrorDetails_CompilerError:
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

	cliLog.Errorf("Script execution error: %s", s.Message)
	return newScriptExecutionError(CodeUnknown, "Script execution error:"+s.Message)
}

func (v *VizierStreamOutputAdapter) getFormattedValue(tableInfo *TableInfo, colIdx int, val interface{}) interface{} {
	if _, ok := tableInfo.latencyCols[colIdx]; ok {
		// If it's a latency col and the data type float64 we will color it based on value.
		if lv, ok := val.(float64); ok {
			return v.latencyColValueToString(lv)
		}
	} else if _, ok := tableInfo.alertCols[colIdx]; ok {
		// If it's a latency col and the data type is bool we will color it based on value.
		if b, ok := val.(bool); ok {
			return v.alertColValueToString(b)
		}
	}
	return val
}

func (v *VizierStreamOutputAdapter) handleExecutionStats(ctx context.Context, es *pl_api_vizierpb.QueryExecutionStats) error {
	v.execStats = es
	return nil
}

func (v *VizierStreamOutputAdapter) handleMutationInfo(ctx context.Context, mi *pl_api_vizierpb.MutationInfo) {
	v.mutationInfo = mi
}

func (v *VizierStreamOutputAdapter) handleData(ctx context.Context, cid string, d *pl_api_vizierpb.ExecuteScriptResponse_Data) error {
	if d.Data.ExecutionStats != nil {
		err := v.handleExecutionStats(ctx, d.Data.ExecutionStats)
		if err != nil {
			return err
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

	numRows := 0
	if d.Data != nil && d.Data.Batch != nil && d.Data.Batch.Cols != nil {
		numRows = getNumRows(d.Data.Batch.Cols[0])
	} else {
		// No records.
		return nil
	}

	cols := d.Data.Batch.Cols
	for rowIdx := 0; rowIdx < numRows; rowIdx++ {
		// Add the cluster ID to the output colums.
		rec := make([]interface{}, len(cols)+1)
		rec[0] = cid
		for colIdx, col := range cols {
			val := v.getNativeTypedValue(tableInfo, rowIdx, colIdx, col.ColData)
			if v.enableFormat {
				rec[colIdx+1] = v.getFormattedValue(tableInfo, colIdx, val)
			} else {
				rec[colIdx+1] = val
			}
		}
		ti, _ := v.tableNameToInfo[tableName]
		if err := ti.w.Write(rec); err != nil {
			return err
		}
	}
	return nil
}

func (v *VizierStreamOutputAdapter) handleMetadata(ctx context.Context, md *pl_api_vizierpb.ExecuteScriptResponse_MetaData) error {
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

	latencyCols := make(map[int]bool)
	alertCols := make(map[int]bool)
	timeColIdx := -1
	// For p50, p99, etc.
	latencyRegex := regexp.MustCompile(`p\d{2}`)

	for idx, col := range relation.Columns {
		if col.ColumnName == "time_" {
			timeColIdx = idx
		}
		if col.ColumnName == "alert" || strings.HasPrefix(col.ColumnName, "alert_") {
			alertCols[idx] = true
		}
		if latencyRegex.Match([]byte(col.ColumnName)) {
			latencyCols[idx] = true
		}
	}

	// Write out the header keys in the order specified by the relation.
	headerKeys := make([]string, len(relation.Columns)+1)
	headerKeys[0] = "_clusterID_"
	for i, col := range relation.Columns {
		headerKeys[i+1] = col.ColumnName
	}
	newWriter.SetHeader(md.MetaData.Name, headerKeys)

	v.tableNameToInfo[tableName] = &TableInfo{
		ID:          tableName,
		w:           newWriter,
		relation:    relation,
		timeColIdx:  timeColIdx,
		latencyCols: latencyCols,
		alertCols:   alertCols,
	}

	return nil
}

func (v *VizierStreamOutputAdapter) isYellowLatency(timeMS float64) bool {
	return (timeMS >= v.latencyYellowThresholdMS) && (timeMS <= v.latencyRedThresholdMS)
}

func (v *VizierStreamOutputAdapter) isRedLatency(timeMS float64) bool {
	return timeMS >= v.latencyRedThresholdMS
}
func (v *VizierStreamOutputAdapter) latencyColValueToString(timeMS float64) string {
	asStr := fmt.Sprintf("%.2f", timeMS)
	if v.isRedLatency(timeMS) {
		return v.redSprintf(asStr)
	}
	if v.isYellowLatency(timeMS) {
		return v.yellowSprintf(asStr)
	}
	return asStr
}

func (v *VizierStreamOutputAdapter) alertColValueToString(val bool) string {
	if val {
		return v.redSprintf("ALERT")
	}
	return ""
}
