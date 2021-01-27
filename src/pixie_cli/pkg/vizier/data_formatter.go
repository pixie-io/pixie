package vizier

import (
	"encoding/json"
	"fmt"
	"math"
	"regexp"
	"sort"
	"strconv"
	"strings"

	"github.com/fatih/color"
	public_vizierapipb "pixielabs.ai/pixielabs/src/api/public/vizierapipb"
)

// For p50, p99, etc.
var latencyRegex = regexp.MustCompile(`(?i)^latency`)
var cpuRegex = regexp.MustCompile(`(?i)^cpu`)

const nanosPerSecond = float64(1000 * 1000 * 1000)

var faintColor = color.New(color.Faint)

// Byte formatter.
// TODO(zasgar): We had to pull this out to separate the units from the values.
// Figure out if we can do this with a library instead.
const (
	Byte = 1 << (iota * 10)
	KiByte
	MiByte
	GiByte
	TiByte
	PiByte
	EiByte
)

var bytesSizeTable = map[string]uint64{
	"b":   Byte,
	"kib": KiByte,
	"mib": MiByte,
	"gib": GiByte,
	"tib": TiByte,
	"pib": PiByte,
	"eib": EiByte,
	// Without suffix
	"":   Byte,
	"ki": KiByte,
	"mi": MiByte,
	"gi": GiByte,
	"ti": TiByte,
	"pi": PiByte,
	"ei": EiByte,
}

func logn(n, b float64) float64 {
	return math.Log(n) / math.Log(b)
}

func humanate(s uint64, base float64, sizes []string, baseUnit string) (string, string) {
	if s < 10 {
		return fmt.Sprintf("%d", s), baseUnit
	}
	e := math.Floor(logn(float64(s), base))
	if int(e) >= len(sizes) {
		e = float64(len(sizes)) - 1
	}
	suffix := sizes[int(e)]
	val := math.Floor(float64(s)/math.Pow(base, e)*10+0.5) / 10
	f := "%.0f"
	if val < base/10 {
		f = "%.1f"
	}

	return fmt.Sprintf(f, val), suffix
}

func iBytes(s uint64) (string, string) {
	sizes := []string{"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB"}
	return humanate(s, 1024, sizes, "B")
}

const (
	nanos   = 1
	micros  = nanos * 1000
	millis  = micros * 1000
	seconds = millis * 1000
)

var durationSizeTable = map[string]uint64{
	"ns": nanos,
	"µs": micros,
	"ms": millis,
	"s":  seconds,
}

func humanizeDuration(t uint64) (string, string) {
	sizes := []string{"ns", "µs", "ms", "s"}
	return humanate(t, 1000, sizes, "s")
}

// DataFormatter formats data for a given Relation.
type DataFormatter interface {
	// FormatValue formats the value for a particular column.
	FormatValue(colIdx int, val interface{}) interface{}
}

type dataFormatterImpl struct {
	latencyYellowThresholdNS float64
	latencyRedThresholdNS    float64
	cpuYellowThreshold       float64
	cpuRedThreshold          float64

	// Internally used functions.
	redSprintf    func(format string, a ...interface{}) string
	yellowSprintf func(format string, a ...interface{}) string
	greenSprintf  func(format string, a ...interface{}) string

	alertCols   map[int]bool
	cpuCols     map[int]bool
	latencyCols map[int]bool

	semanticTypeMap map[int]public_vizierapipb.SemanticType
	dataTypeMap     map[int]public_vizierapipb.DataType
}

// NewDataFormatterForTable creates a new data formatter based on the input relation.
func NewDataFormatterForTable(relation *public_vizierapipb.Relation) DataFormatter {
	alertCols := make(map[int]bool)
	cpuCols := make(map[int]bool)
	latencyCols := make(map[int]bool)
	semanticTypeMap := make(map[int]public_vizierapipb.SemanticType)
	dataTypeMap := make(map[int]public_vizierapipb.DataType)

	for idx, col := range relation.Columns {
		if col.ColumnName == "alert" || strings.HasPrefix(col.ColumnName, "alert_") {
			alertCols[idx] = true
		}
		if cpuRegex.Match([]byte(col.ColumnName)) && col.ColumnSemanticType == public_vizierapipb.ST_PERCENT {
			cpuCols[idx] = true
		}
		if latencyRegex.Match([]byte(col.ColumnName)) && col.ColumnSemanticType == public_vizierapipb.ST_DURATION_NS {
			latencyCols[idx] = true
		}
		semanticTypeMap[idx] = col.ColumnSemanticType
		dataTypeMap[idx] = col.ColumnType
	}

	return &dataFormatterImpl{
		alertCols:                alertCols,
		cpuCols:                  cpuCols,
		latencyCols:              latencyCols,
		semanticTypeMap:          semanticTypeMap,
		dataTypeMap:              dataTypeMap,
		cpuYellowThreshold:       70.0,
		cpuRedThreshold:          80.0,
		latencyYellowThresholdNS: 200.0 * 1000 * 1000,
		latencyRedThresholdNS:    400.0 * 1000 * 1000,
		redSprintf:               color.New(color.FgRed).SprintfFunc(),
		yellowSprintf:            color.New(color.FgYellow).SprintfFunc(),
		greenSprintf:             color.New(color.FgGreen).SprintfFunc(),
	}
}

func toString(val interface{}) string {
	return fmt.Sprintf("%v", val)
}

func (d *dataFormatterImpl) getStringForVal(dt public_vizierapipb.DataType, st public_vizierapipb.SemanticType, val interface{}) string {
	switch st {
	case public_vizierapipb.ST_BYTES:
		return formatBytes(val)
	case public_vizierapipb.ST_DURATION_NS:
		return formatDuration(val)
	case public_vizierapipb.ST_THROUGHPUT_PER_NS:
		return formatThroughput(val)
	case public_vizierapipb.ST_THROUGHPUT_BYTES_PER_NS:
		return formatThroughputBytes(val)
	case public_vizierapipb.ST_HTTP_RESP_STATUS:
		return d.formatRespStatus(val)
	case public_vizierapipb.ST_PERCENT:
		return formatPercent(val)
	case public_vizierapipb.ST_DURATION_NS_QUANTILES:
		return d.formatKV(public_vizierapipb.FLOAT64, public_vizierapipb.ST_DURATION_NS, val)
	case public_vizierapipb.ST_QUANTILES:
		return d.formatKV(public_vizierapipb.FLOAT64, public_vizierapipb.ST_NONE, val)
	}

	if dt == public_vizierapipb.FLOAT64 {
		if floatVal, ok := val.(float64); ok {
			return strconv.FormatFloat(floatVal, 'g', 6, 64)
		}
	}

	// We may want to add logic by data type as well, if no relevant semantic types match.
	return toString(val)
}

func withSign(neg bool, val string) string {
	sign := ""
	if neg {
		sign = "-"
	}
	return fmt.Sprintf(`%s%s`, sign, val)
}

func formatBytesInternal(val float64) string {
	s, units := iBytes(uint64(math.Abs(val)))
	return withSign(val < 0, fmt.Sprintf("%s %s", s, formatUnits(units)))
}

func formatBytes(val interface{}) string {
	if intVal, ok := val.(int64); ok {
		// ST_BYTES path
		return formatBytesInternal(float64(intVal))
	}
	if floatVal, ok := val.(float64); ok {
		// ST_THROUGHPUT_BYTES_PER_NS path
		return formatBytesInternal(floatVal)
	}
	return toString(val)
}

func formatDurationInt(val int64) string {
	v := uint64(val)
	if val < 0 {
		v = uint64(-1 * val)
	}

	s, units := humanizeDuration(v)
	return withSign(val < 0, fmt.Sprintf("%s %s", s, formatUnits(units)))
}

func formatUnits(unit string) string {
	return faintColor.Sprintf("%s", unit)
}

func formatDuration(val interface{}) string {
	if intVal, ok := val.(int64); ok {
		// ST_DURATION_NS path
		return formatDurationInt(intVal)
	}
	if floatVal, ok := val.(float64); ok {
		// ST_DURATION_NS_QUANTILES path
		return formatDurationInt(int64(math.Round(floatVal)))
	}
	return toString(val)
}

func formatThroughput(val interface{}) string {
	floatVal, ok := val.(float64)
	str := toString(val)
	if !ok {
		return str
	}
	perS := floatVal * nanosPerSecond
	return fmt.Sprintf("%f %s", perS, formatUnits("/sec"))
}

func formatThroughputBytes(val interface{}) string {
	floatVal, ok := val.(float64)
	str := toString(val)
	if !ok {
		return str
	}
	perS := floatVal * nanosPerSecond
	return fmt.Sprintf("%s%s", formatBytes(perS), formatUnits("/sec"))
}

func formatPercent(val interface{}) string {
	floatVal, ok := val.(float64)
	str := toString(val)
	if !ok {
		return str
	}
	return fmt.Sprintf("%.2f %s", floatVal*100, formatUnits("%"))
}

func (d *dataFormatterImpl) formatKV(valueDataType public_vizierapipb.DataType, valueSemanticType public_vizierapipb.SemanticType, val interface{}) string {
	strVal, ok := val.(string)
	if !ok {
		return toString(val)
	}
	var result map[string]interface{}
	json.Unmarshal([]byte(strVal), &result)

	var keys []string
	for k := range result {
		keys = append(keys, k)
	}
	sort.Strings(keys)

	var outvals []string
	for _, k := range keys {
		value := d.getStringForVal(valueDataType, valueSemanticType, result[k])
		outvals = append(outvals, fmt.Sprintf("%s: %s", k, value))
	}
	return strings.Join(outvals, ", ")
}

func (d *dataFormatterImpl) formatRespStatus(val interface{}) string {
	intVal, ok := val.(int64)
	str := toString(val)
	if !ok {
		return str
	}
	if intVal < 200 {
		return str
	}
	if intVal < 300 {
		return d.greenSprintf(str)
	}
	if intVal < 400 {
		return str
	}
	return d.redSprintf(str)
}

func (d *dataFormatterImpl) formatCPUColor(stringVal string, floatVal float64) string {
	escaped := strings.Replace(stringVal, "%", "%%", 1)
	if floatVal > d.cpuRedThreshold {
		return d.redSprintf(escaped)
	}
	if floatVal > d.cpuYellowThreshold {
		return d.yellowSprintf(escaped)
	}
	return d.greenSprintf(escaped)
}

func (d *dataFormatterImpl) formatLatencyColor(stringVal string, floatVal float64) string {
	if floatVal > d.latencyRedThresholdNS {
		return d.redSprintf(stringVal)
	}
	if floatVal > d.latencyYellowThresholdNS {
		return d.yellowSprintf(stringVal)
	}
	return d.greenSprintf(stringVal)
}

func (d *dataFormatterImpl) formatAlert(val bool) string {
	if val {
		return d.redSprintf("ALERT")
	}
	return ""
}

func (d *dataFormatterImpl) FormatValue(colIdx int, val interface{}) interface{} {
	// First get the string representation of the value, as determined by the semantic type and data type.
	stringVal := d.getStringForVal(d.dataTypeMap[colIdx], d.semanticTypeMap[colIdx], val)

	// Now add color coding if certain keywords appear in the name of the column.
	if _, ok := d.latencyCols[colIdx]; ok {
		// If it's a latency col and the data type float64 or int64 we will color it based on value.
		if floatVal, isFloat64 := val.(float64); isFloat64 {
			return d.formatLatencyColor(stringVal, floatVal)
		}
		if intVal, isInt64 := val.(int64); isInt64 {
			return d.formatLatencyColor(stringVal, float64(intVal))
		}
	} else if _, ok := d.alertCols[colIdx]; ok {
		// If it's an alert col, change the formatting to be text w/ colors.
		if b, ok := val.(bool); ok {
			return d.formatAlert(b)
		}
	} else if _, ok := d.cpuCols[colIdx]; ok {
		// If it's an alert col, change the formatting to be text w/ colors.
		if floatVal, ok := val.(float64); ok {
			return d.formatCPUColor(stringVal, floatVal)
		}
	}

	return stringVal
}
