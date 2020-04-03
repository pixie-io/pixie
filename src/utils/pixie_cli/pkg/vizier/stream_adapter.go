package vizier

import (
	"context"
	"encoding/binary"
	"errors"
	"fmt"
	"io"
	"os"
	"regexp"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/fatih/color"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/components"
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
	Resp *pl_api_vizierpb.ExecuteScriptResponse
	Err  error
}

// VizierStreamOutputAdapter adapts the vizier output to the StreamWriters.
type VizierStreamOutputAdapter struct {
	tableNameToInfo     map[string]*TableInfo
	streamWriterFactory StreamWriterFactorFunc
	wg                  sync.WaitGroup
	enableFormat        bool

	latencyYellowThresholdMS float64
	latencyRedThresholdMS    float64

	// Internally used functions.
	redSprintf    func(format string, a ...interface{}) string
	yellowSprintf func(format string, a ...interface{}) string
}

var (
	// ErrMetadataMissing is returned when table was malformed missing data.
	ErrMetadataMissing = errors.New("metadata missing for table")
	// ErrDuplicateMetadata is returned when table is malformed an contains multiple metadata.
	ErrDuplicateMetadata = errors.New("duplicate table metadata received")
)

// NewVizierStreamOutputAdapter creates a new vizier output adapter.
func NewVizierStreamOutputAdapter(ctx context.Context, stream chan *VizierExecData, format string) *VizierStreamOutputAdapter {
	enableFormat := format != "json"

	factoryFunc := func(md *pl_api_vizierpb.ExecuteScriptResponse_MetaData) components.OutputStreamWriter {
		return components.CreateStreamWriter(format, os.Stdout)
	}

	adapter := &VizierStreamOutputAdapter{
		tableNameToInfo:     make(map[string]*TableInfo),
		streamWriterFactory: factoryFunc,
		enableFormat:        enableFormat,

		latencyYellowThresholdMS: 200,
		latencyRedThresholdMS:    400,
		redSprintf:               color.New(color.FgRed).SprintfFunc(),
		yellowSprintf:            color.New(color.FgYellow).SprintfFunc(),
	}

	adapter.wg.Add(1)
	go adapter.handleStream(ctx, stream)

	return adapter
}

// Finish must be called to wait for the output and flush all the data.
func (v *VizierStreamOutputAdapter) Finish() {
	v.wg.Wait()
	for _, ti := range v.tableNameToInfo {
		ti.w.Finish()
	}
}

func (v *VizierStreamOutputAdapter) handleStream(ctx context.Context, stream chan *VizierExecData) {
	defer v.wg.Done()
	for {
		select {
		case <-ctx.Done():
			return
		case msg := <-stream:
			if msg == nil {
				return
			}
			if msg.Err != nil {
				if msg.Err == io.EOF {
					return
				}
				return
			}

			if msg.Resp.Status != nil && msg.Resp.Status.Code != 0 {
				// Try to parse the error and return it up stream.
				v.printErrorAndDie(ctx, msg.Resp.Status)
			}
			var err error
			switch res := msg.Resp.Result.(type) {
			case *pl_api_vizierpb.ExecuteScriptResponse_MetaData:
				err = v.handleMetadata(ctx, res)
			case *pl_api_vizierpb.ExecuteScriptResponse_Data:
				err = v.handleData(ctx, res)
			default:
				panic("unhandled response type")
			}
			if err != nil {
				log.WithError(err).Fatalln("Failed to handle data from Vizier")
			}
		}
	}
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
	case *pl_api_vizierpb.Column_Duration64NsData:
		return len(u.Duration64NsData.Data)
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
	case *pl_api_vizierpb.Column_Duration64NsData:
		return u.Duration64NsData.Data[rowIdx]
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
func (v *VizierStreamOutputAdapter) printErrorAndDie(ctx context.Context, s *pl_api_vizierpb.Status) {
	if s.Message != "" {
		fmt.Fprint(os.Stderr, "\nError: %s\n", s.Message)
	}

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
		fmt.Fprintf(os.Stderr, "\nScript compilation failed: \n")
		for _, s := range compilerErrors {
			fmt.Fprintf(os.Stderr, s)
		}
	}

	fmt.Fprint(os.Stderr, "\n")
	log.Fatal("Script execution error")
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

func (v *VizierStreamOutputAdapter) handleData(ctx context.Context, d *pl_api_vizierpb.ExecuteScriptResponse_Data) error {

	if d.Data.Batch == nil {
		return nil
	}
	tableID := d.Data.Batch.TableID
	tableInfo, ok := v.tableNameToInfo[tableID]
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
		rec := make([]interface{}, len(cols))
		for colIdx, col := range cols {
			val := v.getNativeTypedValue(tableInfo, rowIdx, colIdx, col.ColData)
			if v.enableFormat {
				rec[colIdx] = v.getFormattedValue(tableInfo, colIdx, val)
			} else {
				rec[colIdx] = val
			}
		}
		ti, _ := v.tableNameToInfo[d.Data.Batch.TableID]
		if err := ti.w.Write(rec); err != nil {
			return err
		}
	}
	return nil
}

func (v *VizierStreamOutputAdapter) handleMetadata(ctx context.Context, md *pl_api_vizierpb.ExecuteScriptResponse_MetaData) error {
	tableID := md.MetaData.ID
	newWriter := v.streamWriterFactory(md)

	if _, exists := v.tableNameToInfo[tableID]; exists {
		return ErrDuplicateMetadata
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
	headerKeys := make([]string, len(relation.Columns))
	for i, col := range relation.Columns {
		headerKeys[i] = col.ColumnName
	}

	newWriter.SetHeader(md.MetaData.Name, headerKeys)

	v.tableNameToInfo[tableID] = &TableInfo{
		ID:          tableID,
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
