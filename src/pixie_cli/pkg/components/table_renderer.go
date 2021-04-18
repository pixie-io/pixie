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

package components

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"strings"
	"time"

	"github.com/olekukonko/tablewriter"
)

// OutputStreamWriter is the default interface for all output writers.
type OutputStreamWriter interface {
	SetHeader(id string, headerValues []string)
	Write(data []interface{}) error
	Finish()
}

// TableView is the interface the provides read access to underlying table data.
type TableView interface {
	Name() string
	Header() []string
	Data() [][]interface{}
}

// CreateStreamWriter creates a formatted writer with the default options.
func CreateStreamWriter(format string, w io.Writer) OutputStreamWriter {
	switch format {
	case "json":
		return NewJSONStreamWriter(w)
	case "table":
		return NewTableStreamWriter(w)
	case "csv":
		return NewCSVStreamWriter(w)
	case "null":
		return &NullStreamWriter{}
	case "inmemory":
		return NewTableAccumulator()
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

func stringifyValue(val interface{}) string {
	switch u := val.(type) {
	case time.Time:
		return u.Format(time.RFC3339)
	case stringer:
		return u.String()
	case float64:
		return fmt.Sprintf("%0.2f", u)
	default:
		return fmt.Sprintf("%+v", u)
	}
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
		s[i] = stringifyValue(val)
	}
	return s
}

// Finish is called when all the data has been sent. In the case of the table we can now render all the values.
func (t *TableStreamWriter) Finish() {
	fmt.Printf("Table ID: %s\n", t.id)
	table := tablewriter.NewWriter(t.w)
	table.SetHeader(t.headerValues)

	table.SetAutoFormatHeaders(true)
	table.SetAutoWrapText(false)
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

// NullStreamWriter reads the data but does not output it.
type NullStreamWriter struct{}

// SetHeader is called to set the key values for each of the data values. Must be called before Write is.
func (*NullStreamWriter) SetHeader(id string, headerValues []string) {}

// Write is called for each record of data.
func (*NullStreamWriter) Write(data []interface{}) error { return nil }

// Finish is called to flush all the data.
func (*NullStreamWriter) Finish() {}

// TableAccumulator accumulates all data from the table.
type TableAccumulator struct {
	id           string
	headerValues []string
	data         [][]interface{}
}

// NewTableAccumulator creates a table accumulator based on input stream.
func NewTableAccumulator() *TableAccumulator {
	return &TableAccumulator{
		data: make([][]interface{}, 0),
	}
}

// SetHeader is called to set the key values for each of the data values. Must be called before Write is.
func (t *TableAccumulator) SetHeader(id string, headerValues []string) {
	t.id = id
	t.headerValues = headerValues
}

// Write is called for each record of data.
func (t *TableAccumulator) Write(data []interface{}) error {
	if len(data) != len(t.headerValues) {
		return errors.New("header/data length mismatch")
	}
	t.data = append(t.data, data)
	return nil
}

// Finish is called when all the data has been sent. For table accumulator it's a noop.
func (t *TableAccumulator) Finish() {
}

// Name returns the table name.
func (t *TableAccumulator) Name() string {
	return t.id
}

// Header returns the header values as a string.
func (t *TableAccumulator) Header() []string {
	return t.headerValues
}

// Data returns the underlying data table as slice of slices.
func (t *TableAccumulator) Data() [][]interface{} {
	return t.data
}

// CSVStreamWriter writes data in CSV format.
type CSVStreamWriter struct {
	w             io.Writer
	delimiter     []byte
	id            string
	headerValues  []string
	headerWritten bool
}

// NewCSVStreamWriter creates a CSVStreamWriter.
func NewCSVStreamWriter(w io.Writer) *CSVStreamWriter {
	return &CSVStreamWriter{w: w, delimiter: []byte{','}, headerWritten: false}
}

// SetHeader is called to set the key values for each of the data values. Must be called before Write is.
func (c *CSVStreamWriter) SetHeader(id string, headerValues []string) {
	c.id = id
	c.headerValues = headerValues
}

func (c *CSVStreamWriter) writeHeader() error {
	buf := &bytes.Buffer{}
	buf.WriteString("table_id")
	for _, headerVal := range c.headerValues {
		buf.Write(c.delimiter)
		buf.WriteString(headerVal)
	}
	buf.Write([]byte{'\n'})
	_, err := c.w.Write(buf.Bytes())
	return err
}

// Write is called for each record of data.
func (c *CSVStreamWriter) Write(data []interface{}) error {
	if !c.headerWritten {
		if err := c.writeHeader(); err != nil {
			return err
		}
		c.headerWritten = true
	}

	if len(data) != len(c.headerValues) {
		return errors.New("header/data length mismatch")
	}
	buf := &bytes.Buffer{}
	buf.WriteString(c.id)
	for _, d := range data {
		buf.Write(c.delimiter)
		dataStr := stringifyValue(d)
		// Add surrounding quotes to any fields that contain commas or newlines.
		if strings.Contains(dataStr, ",") || strings.Contains(dataStr, "\n") {
			// CSV escapes quotes by double quoting.
			dataStr = strings.Replace(dataStr, "\"", "\"\"", -1)
			dataStr = "\"" + dataStr + "\""
		}
		buf.WriteString(dataStr)
	}
	buf.Write([]byte{'\n'})
	_, err := c.w.Write(buf.Bytes())
	return err
}

// Finish is called to flush all the data.
func (c *CSVStreamWriter) Finish() {
	// Since CSV writer outputs records right away there is nothing to do here.
}
