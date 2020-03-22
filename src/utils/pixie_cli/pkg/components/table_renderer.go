package components

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"time"

	"github.com/olekukonko/tablewriter"
)

// OutputStreamWriter is the default interface for all output writers.
type OutputStreamWriter interface {
	SetHeader(id string, headerValues []string)
	Write(data []interface{}) error
	Finish()
}

// CreateStreamWriter creates a formatted writer with the default options.
func CreateStreamWriter(format string, w io.Writer) OutputStreamWriter {
	switch format {
	case "json":
		return NewJSONStreamWriter(w)
	case "table":
		return NewTableStreamWriter(w)
	default:
		return NewTableStreamWriter(w)
	}
}

// TableStreamWriter writer output in tabular format. It's blocking so data is only written after the table is complete.
type TableStreamWriter struct {
	w            io.Writer
	id           string
	headerValues []string
	data         [][]interface{}
}

type stringer interface {
	String() string
}

// NewTableStreamWriter creates a table writer based on input stream.
func NewTableStreamWriter(w io.Writer) *TableStreamWriter {
	return &TableStreamWriter{
		w:    w,
		data: make([][]interface{}, 0),
	}
}

// SetHeader is called to set the key values for each of the data values. Must be called before Write is.
func (t *TableStreamWriter) SetHeader(id string, headerValues []string) {
	t.id = id
	t.headerValues = headerValues
}

// Write is called for each record of data.
func (t *TableStreamWriter) Write(data []interface{}) error {
	if len(data) != len(t.headerValues) {
		return errors.New("header/data length mismatch")
	}
	t.data = append(t.data, data)
	return nil
}
func (t *TableStreamWriter) stringifyRow(row []interface{}) []string {
	s := make([]string, len(row))

	for i, val := range row {
		switch u := val.(type) {
		case time.Time:
			s[i] = u.Format(time.RFC3339)
		case stringer:
			s[i] = u.String()
		case float64:
			s[i] = fmt.Sprintf("%0.2f", u)
		default:
			s[i] = fmt.Sprintf("%+v", u)
		}
	}
	return s
}

// Finish is called when all the data has been sent. In the case of the table we can now render all the values.
func (t *TableStreamWriter) Finish() {
	fmt.Printf("Table ID: %s\n", t.id)
	table := tablewriter.NewWriter(t.w)
	table.SetHeader(t.headerValues)

	table.SetAutoFormatHeaders(true)
	table.SetHeaderAlignment(tablewriter.ALIGN_LEFT)
	table.SetAlignment(tablewriter.ALIGN_LEFT)
	table.SetColWidth(30)
	table.SetReflowDuringAutoWrap(true)
	table.SetCenterSeparator("")
	table.SetColumnSeparator("")
	table.SetRowSeparator("")
	table.SetHeaderLine(false)
	table.SetBorder(false)
	table.SetTablePadding("\t")
	table.SetNoWhiteSpace(false)

	for _, row := range t.data {
		table.Append(t.stringifyRow(row))
	}

	table.Render()
}

const tableNameKey = "_tableName_"

// MapItem is a key, value pain.
type MapItem struct {
	Key, Value interface{}
}

// MapSlice is an array of items.
type MapSlice []MapItem

// MarshalJSON marshals the k/v array in order.
func (ms MapSlice) MarshalJSON() ([]byte, error) {
	buf := &bytes.Buffer{}
	buf.Write([]byte{'{'})
	for i, mi := range ms {
		b, err := json.Marshal(&mi.Value)
		if err != nil {
			return nil, err
		}
		buf.WriteString(fmt.Sprintf("%q:", fmt.Sprintf("%v", mi.Key)))
		buf.Write(b)
		if i < len(ms)-1 {
			buf.Write([]byte{','})
		}
	}
	buf.Write([]byte{'}'})
	return buf.Bytes(), nil
}

// JSONStreamWriter writes one json record per row.
type JSONStreamWriter struct {
	w            io.Writer
	id           string
	headerValues []string
	encoder      *json.Encoder
}

// NewJSONStreamWriter creates a JSONStreamWriter
func NewJSONStreamWriter(w io.Writer) *JSONStreamWriter {
	return &JSONStreamWriter{w: w, encoder: json.NewEncoder(w)}
}

// SetHeader is called to set the key values for each of the data values. Must be called before Write is.
func (j *JSONStreamWriter) SetHeader(id string, headerValues []string) {
	j.id = id
	j.headerValues = headerValues
}

// Write is called for each record of data.
func (j *JSONStreamWriter) Write(data []interface{}) error {
	if len(data) != len(j.headerValues) {
		return errors.New("header/data length mismatch")
	}

	val := make([]MapItem, len(data)+1) // +1 for the table name
	val[0].Key = tableNameKey
	val[0].Value = j.id

	for i, d := range data {
		val[i+1].Key = j.headerValues[i]
		val[i+1].Value = d
	}

	return j.encoder.Encode(MapSlice(val))
}

// Finish is called to flush all the data.
func (j *JSONStreamWriter) Finish() {
	// Since JSON writer outputs records right away there is nothing to do here.
}
