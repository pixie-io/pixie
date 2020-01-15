package components

import (
	"encoding/binary"
	"encoding/json"
	"fmt"
	"os"
	"regexp"
	"sort"
	"strings"
	"time"

	"github.com/TylerBrock/colorjson"
	"github.com/fatih/color"
	"github.com/olekukonko/tablewriter"
	uuid "github.com/satori/go.uuid"
	schemapb "pixielabs.ai/pixielabs/src/table_store/proto"
)

// Function getNumRows returns the number of rows in the input column.
func getNumRows(in *schemapb.Column) int {
	switch u := in.ColData.(type) {
	case *schemapb.Column_StringData:
		return len(u.StringData.Data)
	case *schemapb.Column_Float64Data:
		return len(u.Float64Data.Data)
	case *schemapb.Column_Int64Data:
		return len(u.Int64Data.Data)
	case *schemapb.Column_Time64NsData:
		return len(u.Time64NsData.Data)
	case *schemapb.Column_BooleanData:
		return len(u.BooleanData.Data)
	case *schemapb.Column_Uint128Data:
		return len(u.Uint128Data.Data)
	}

	return 0
}

// TableRenderer is used to render data tables.
type TableRenderer struct {
	printBatchIndex           bool
	enableAlertColorization   bool
	enableLatencyColorization bool
	latencyYellowThresholdMS  float64
	latencyRedThresholdMS     float64

	// Internally used functions.
	redSprintf    func(format string, a ...interface{}) string
	yellowSprintf func(format string, a ...interface{}) string
}

// NewTableRenderer creates a TableRenderer with default settings.
func NewTableRenderer() *TableRenderer {
	return &TableRenderer{
		printBatchIndex:           false,
		enableAlertColorization:   true,
		enableLatencyColorization: true,
		latencyYellowThresholdMS:  200,
		latencyRedThresholdMS:     400,
		redSprintf:                color.New(color.FgRed).SprintfFunc(),
		yellowSprintf:             color.New(color.FgYellow).SprintfFunc(),
	}
}

func (r *TableRenderer) isYellowLatency(timeMS float64) bool {
	return r.enableLatencyColorization && (timeMS >= r.latencyYellowThresholdMS) && (timeMS <= r.latencyRedThresholdMS)
}

func (r *TableRenderer) isRedLatency(timeMS float64) bool {
	return r.enableLatencyColorization && (timeMS >= r.latencyRedThresholdMS)
}

func (r *TableRenderer) latencyColValueToString(timeMS float64) string {
	asStr := fmt.Sprintf("%.2f", timeMS)
	if r.isRedLatency(timeMS) {
		return r.redSprintf(asStr)
	}
	if r.isYellowLatency(timeMS) {
		return r.yellowSprintf(asStr)
	}
	return asStr
}

func (r *TableRenderer) alertColValueToString(val bool) string {
	if val {
		return r.redSprintf("ALERT")
	}
	return ""
}

func (r *TableRenderer) prettyStringValue(val string) string {
	// If json parsing works, then return colorized json string.
	// otherwise return normal string.
	var obj map[string]interface{}
	err := json.Unmarshal([]byte(val), &obj)
	if err != nil {
		return val
	}
	f := colorjson.NewFormatter()
	f.Indent = 0
	f.DisabledColor = false
	s, err := f.Marshal(obj)
	if err == nil {
		return string(s)
	}
	return val
}

func (r *TableRenderer) getRowBatchRowDataAsArray(in *schemapb.RowBatchData, batchIdx, rowIdx, timeColIdx int) []interface{} {

	row := make([]interface{}, 0)
	numCols := len(in.Cols)
	if r.printBatchIndex {
		row = append(row, batchIdx)
	}
	for colIdx := 0; colIdx < numCols; colIdx++ {
		switch u := in.Cols[colIdx].ColData.(type) {
		case *schemapb.Column_StringData:
			row = append(row, string(u.StringData.Data[rowIdx]))
		case *schemapb.Column_Float64Data:
			row = append(row, float64(u.Float64Data.Data[rowIdx]))
		case *schemapb.Column_Int64Data:
			// TODO(zasgar): We really should not need this, but some of our operations convert time
			// to int64. We need to maintain types in the engine/compiler so that proper type casting can be done.
			if colIdx == timeColIdx {
				row = append(row, time.Unix(0, u.Int64Data.Data[rowIdx]))
			} else {
				row = append(row, u.Int64Data.Data[rowIdx])
			}
		case *schemapb.Column_Time64NsData:
			row = append(row, time.Unix(0, u.Time64NsData.Data[rowIdx]))
		case *schemapb.Column_BooleanData:
			row = append(row, bool(u.BooleanData.Data[rowIdx]))
		case *schemapb.Column_Uint128Data:
			b := make([]byte, 16)
			b2 := b[8:]
			binary.BigEndian.PutUint64(b, u.Uint128Data.Data[rowIdx].High)
			binary.BigEndian.PutUint64(b2, u.Uint128Data.Data[rowIdx].Low)
			row = append(row, uuid.FromBytesOrNil(b))
		}
	}
	return row
}

func (r *TableRenderer) dataToString(data [][]interface{}, latencyCols map[int]bool, alertCols map[int]bool) [][]string {
	var out [][]string
	for rowIdx := 0; rowIdx < len(data); rowIdx++ {
		row := make([]string, 0)
		for colIdx := 0; colIdx < len(data[rowIdx]); colIdx++ {
			switch u := data[rowIdx][colIdx].(type) {
			case time.Time:
				row = append(row, fmt.Sprintf("%s", u.String()))
			case bool:
				if alertCols[colIdx] {
					row = append(row, r.alertColValueToString(u))
				} else {
					row = append(row, fmt.Sprintf("%t", u))
				}
			case int64:
				row = append(row, fmt.Sprintf("%d", u))
			case int:
				row = append(row, fmt.Sprintf("%d", u))
			case float64:
				if latencyCols[colIdx] {
					row = append(row, r.latencyColValueToString(u))
				} else {
					row = append(row, fmt.Sprintf("%.2f", u))
				}
			case string:
				row = append(row, r.prettyStringValue(u))
			case uuid.UUID:
				row = append(row, u.String())
			default:
				panic(fmt.Sprintf("Dont know about type: %T", u))
			}
		}
		out = append(out, row)
	}
	return out
}

// RenderTable will convert the proto table to a tabular format and print it to the string.
func (r *TableRenderer) RenderTable(t *schemapb.Table) {
	relation := t.Relation
	in := t.RowBatches
	if len(in) == 0 {
		fmt.Println("Empty table ...")
		return
	}

	// Find time/latency/alert cols.
	latencyCols := make(map[int]bool)
	alertCols := make(map[int]bool)
	timeColIdx := -1
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

	// We read the data out into an array of array of interfaces. This converts the
	// columnar data to row based data.
	var data [][]interface{}
	for batchIdx, rb := range in {
		numRows := getNumRows(in[batchIdx].Cols[0])
		for rowIdx := 0; rowIdx < numRows; rowIdx++ {
			data = append(data, r.getRowBatchRowDataAsArray(rb, batchIdx, rowIdx, timeColIdx))
		}
	}

	// Sort the columns from left to right. We do this by doing stable sorts from right to left
	// to get the partial ordering right. This function can be made more efficient my moving out the
	// type casts, but it's unlikely to be in the critical path because of the small data.
	numCols := len(relation.Columns)
	for sortIdx := numCols - 1; sortIdx >= 0; sortIdx-- {
		sort.SliceStable(data, func(i, j int) bool {
			row1 := data[i]
			row2 := data[j]
			idx := sortIdx
			switch u1 := row1[idx].(type) {
			case bool:
				if u1 == false && row2[idx].(bool) == true {
					return true
				}
			case int64:
				if u1 < row2[idx].(int64) {
					return true
				}
			case int:
				if u1 < row2[idx].(int) {
					return true
				}
			case float64:
				if u1 < row2[idx].(float64) {
					return true
				}
			case string:
				if u1 < row2[idx].(string) {
					return true
				}
			case time.Time:
				if u1.Unix() < row2[idx].(time.Time).Unix() {
					return true
				}
			case uuid.UUID:
				return u1.String() < row2[idx].(uuid.UUID).String()
			default:
				panic(fmt.Sprintf("Dont know abou type: %T", u1))
			}
			return false
		})
	}

	table := tablewriter.NewWriter(os.Stdout)
	table.SetColWidth(100)
	table.SetAutoWrapText(false)
	header := make([]string, 0)
	if r.printBatchIndex {
		header = append(header, "BatchIdx")
	}
	for _, cols := range relation.Columns {
		header = append(header, cols.ColumnName)
	}
	table.SetHeader(header)
	table.AppendBulk(r.dataToString(data, latencyCols, alertCols))
	table.Render()
}
