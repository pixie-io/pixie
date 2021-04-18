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

package main

import (
	"context"
	"fmt"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"

	"px.dev/pixie/src/api/go/pxapi/types"
	vizierpb "px.dev/pixie/src/api/public/vizierapipb"
)

func makeTableMetadata(schemaTypes ...types.DataType) *types.TableMetadata {
	var tableColSchemas []types.ColSchema
	for idx, schemaType := range schemaTypes {
		tableColSchemas = append(tableColSchemas,
			types.ColSchema{
				Name:         fmt.Sprintf("Column %d", idx),
				Type:         schemaType,
				SemanticType: vizierpb.ST_NONE,
			},
		)
	}

	colIdxByName := make(map[string]int64)
	for idx, col := range tableColSchemas {
		colIdxByName[col.Name] = int64(idx)
	}

	return &types.TableMetadata{
		Name:         "Response Table",
		ColInfo:      tableColSchemas,
		ColIdxByName: colIdxByName,
	}
}

func tableMuxAcceptTableAndHandleRecord(t *testing.T, tm *PixieToGrafanaTableMux,
	tableOneMetadata *types.TableMetadata, recordLst []*types.Record) {
	ctx := context.Background()

	tableRecordHandler, err := tm.AcceptTable(ctx, *tableOneMetadata)
	assert.Nil(t, err)

	err = tableRecordHandler.HandleInit(ctx, *tableOneMetadata)
	assert.Nil(t, err)

	// Loop through each record (each row)
	for _, record := range recordLst {
		err = tableRecordHandler.HandleRecord(ctx, record)
		assert.Nil(t, err)
	}

	err = tableRecordHandler.HandleDone(ctx)
	assert.Nil(t, err)
}

func TestEmptyTable(t *testing.T) {
	tableOneMetadata := makeTableMetadata(vizierpb.FLOAT64)
	var recordLst []*types.Record
	tm := &PixieToGrafanaTableMux{}
	tableMuxAcceptTableAndHandleRecord(t, tm, tableOneMetadata, recordLst)
	assert.Equal(t, 1, len(tm.pxTablePrinterLst))
	grafanaFrame := tm.pxTablePrinterLst[0].frame

	// Check the number of fields in frame.
	assert.Equal(t, 1, len(grafanaFrame.Fields))
	assert.Equal(t, 0, grafanaFrame.Fields[0].Len())
}

func TestOneStringColumn(t *testing.T) {
	tableOneMetadata := makeTableMetadata(vizierpb.STRING)
	rowVals := []string{
		"First Val",
		"Second Val",
		"Third Val",
	}

	// Slice of records. Each has a single entry of a string.
	var recordLst []*types.Record
	for _, rowVal := range rowVals {
		var dataStrLst []types.Datum

		newStringVal := types.NewStringValue(&tableOneMetadata.ColInfo[0])
		newStringVal.ScanString(rowVal)
		dataStrLst = append(dataStrLst, newStringVal)

		recordLst = append(recordLst, &types.Record{
			Data:          dataStrLst,
			TableMetadata: tableOneMetadata,
		})
	}

	tm := &PixieToGrafanaTableMux{}
	tableMuxAcceptTableAndHandleRecord(t, tm, tableOneMetadata, recordLst)

	assert.Equal(t, 1, len(tm.pxTablePrinterLst))
	grafanaFrame := tm.pxTablePrinterLst[0].frame

	// Check the number of fields in frame.
	assert.Equal(t, 1, len(grafanaFrame.Fields))

	for idx, expectedRowValStr := range rowVals {
		val := grafanaFrame.Fields[0].At(idx).(string)
		assert.Equal(t, val, expectedRowValStr)
	}
}

func TestOneInt64Column(t *testing.T) {
	tableOneMetadata := makeTableMetadata(vizierpb.INT64)
	rowVals := []int64{
		124,
		-165,
		1324234,
	}

	var recordLst []*types.Record
	for _, rowVal := range rowVals {
		var dataStrLst []types.Datum

		newIntVal := types.NewInt64Value(&tableOneMetadata.ColInfo[0])
		newIntVal.ScanInt64(rowVal)
		dataStrLst = append(dataStrLst, newIntVal)

		recordLst = append(recordLst, &types.Record{
			Data:          dataStrLst,
			TableMetadata: tableOneMetadata,
		})
	}

	tm := &PixieToGrafanaTableMux{}
	tableMuxAcceptTableAndHandleRecord(t, tm, tableOneMetadata, recordLst)

	assert.Equal(t, 1, len(tm.pxTablePrinterLst))
	grafanaFrame := tm.pxTablePrinterLst[0].frame

	// Check the number of fields in frame.
	assert.Equal(t, 1, len(grafanaFrame.Fields))

	for idx, expectedRowValStr := range rowVals {
		val := grafanaFrame.Fields[0].At(idx).(int64)
		assert.Equal(t, val, expectedRowValStr)
	}
}

func TestOneBoolColumn(t *testing.T) {
	tableOneMetadata := makeTableMetadata(vizierpb.BOOLEAN)
	rowVals := []bool{
		false,
		true,
		false,
	}

	var recordLst []*types.Record
	for _, rowVal := range rowVals {
		var dataStrLst []types.Datum

		newBoolVal := types.NewBooleanValue(&tableOneMetadata.ColInfo[0])
		newBoolVal.ScanBool(rowVal)
		dataStrLst = append(dataStrLst, newBoolVal)

		recordLst = append(recordLst, &types.Record{
			Data:          dataStrLst,
			TableMetadata: tableOneMetadata,
		})
	}

	tm := &PixieToGrafanaTableMux{}
	tableMuxAcceptTableAndHandleRecord(t, tm, tableOneMetadata, recordLst)

	assert.Equal(t, 1, len(tm.pxTablePrinterLst))
	grafanaFrame := tm.pxTablePrinterLst[0].frame

	// Check the number of fields in frame.
	assert.Equal(t, 1, len(grafanaFrame.Fields))

	for idx, expectedRowValStr := range rowVals {
		val := grafanaFrame.Fields[0].At(idx).(bool)
		assert.Equal(t, val, expectedRowValStr)
	}
}

func TestOneFloat64Column(t *testing.T) {
	tableOneMetadata := makeTableMetadata(vizierpb.FLOAT64)
	rowVals := []float64{
		124.02345,
		-165.9999,
		1324234.1234,
	}

	var recordLst []*types.Record
	for _, rowVal := range rowVals {
		var dataStrLst []types.Datum

		newFloatVal := types.NewFloat64Value(&tableOneMetadata.ColInfo[0])
		newFloatVal.ScanFloat64(rowVal)
		dataStrLst = append(dataStrLst, newFloatVal)

		recordLst = append(recordLst, &types.Record{
			Data:          dataStrLst,
			TableMetadata: tableOneMetadata,
		})
	}

	tm := &PixieToGrafanaTableMux{}
	tableMuxAcceptTableAndHandleRecord(t, tm, tableOneMetadata, recordLst)

	assert.Equal(t, 1, len(tm.pxTablePrinterLst))
	grafanaFrame := tm.pxTablePrinterLst[0].frame

	// Check the number of fields in frame.
	assert.Equal(t, 1, len(grafanaFrame.Fields))

	for idx, expectedRowValStr := range rowVals {
		val := grafanaFrame.Fields[0].At(idx).(float64)
		assert.Equal(t, val, expectedRowValStr)
	}
}

func TestOneUInt128Column(t *testing.T) {
	tableOneMetadata := makeTableMetadata(vizierpb.UINT128)
	newUInt128Val := types.NewUint128Value(&tableOneMetadata.ColInfo[0])
	data := &vizierpb.UInt128{
		Low:  0x0011223344556677,
		High: 0x8811223344556677,
	}
	newUInt128Val.ScanUInt128(data)

	dataStrLst := []types.Datum{
		newUInt128Val,
	}

	recordLst := []*types.Record{
		{
			Data:          dataStrLst,
			TableMetadata: tableOneMetadata,
		},
	}

	tm := &PixieToGrafanaTableMux{}
	tableMuxAcceptTableAndHandleRecord(t, tm, tableOneMetadata, recordLst)

	assert.Equal(t, 1, len(tm.pxTablePrinterLst))
	grafanaFrame := tm.pxTablePrinterLst[0].frame

	// Check the number of fields in frame.
	assert.Equal(t, 1, len(grafanaFrame.Fields))

	expectedRowValStr := "88112233-4455-6677-0011-223344556677"
	val := grafanaFrame.Fields[0].At(0).(string)
	assert.Equal(t, val, expectedRowValStr)
}

func TestOneTimeColumn(t *testing.T) {
	tableOneMetadata := makeTableMetadata(vizierpb.TIME64NS)
	rowVals := []time.Time{
		time.Now(),
		time.Now().Add(1 * time.Hour),
		time.Now().Add(-2 * time.Hour),
	}

	var recordLst []*types.Record
	for _, rowVal := range rowVals {
		var dataStrLst []types.Datum

		newTime64NSVal := types.NewTime64NSValue(&tableOneMetadata.ColInfo[0])
		newTime64NSVal.ScanInt64(rowVal.UnixNano())
		dataStrLst = append(dataStrLst, newTime64NSVal)

		recordLst = append(recordLst, &types.Record{
			Data:          dataStrLst,
			TableMetadata: tableOneMetadata,
		})
	}

	tm := &PixieToGrafanaTableMux{}
	tableMuxAcceptTableAndHandleRecord(t, tm, tableOneMetadata, recordLst)

	assert.Equal(t, 1, len(tm.pxTablePrinterLst))
	grafanaFrame := tm.pxTablePrinterLst[0].frame

	// Check the number of fields in frame.
	assert.Equal(t, 1, len(grafanaFrame.Fields))

	for idx, expectedRowValStr := range rowVals {
		val := grafanaFrame.Fields[0].At(idx).(time.Time)
		assert.True(t, val.Equal(expectedRowValStr))
	}
}

func TestTwoColumn(t *testing.T) {
	tableOneMetadata := makeTableMetadata(vizierpb.STRING,
		vizierpb.INT64)
	rowStringVals := []string{
		"First Val",
		"Second Val",
		"Third Val",
	}

	rowIntVals := []int64{
		124,
		-165,
		1324234,
	}

	// Slice of records.
	var recordLst []*types.Record
	for idx, rowVal := range rowStringVals {
		var dataStrLst []types.Datum

		// Add string value to column
		newStringVal := types.NewStringValue(&tableOneMetadata.ColInfo[0])
		newStringVal.ScanString(rowVal)
		dataStrLst = append(dataStrLst, newStringVal)

		newInt64Val := types.NewInt64Value(&tableOneMetadata.ColInfo[1])
		newInt64Val.ScanInt64(rowIntVals[idx])
		dataStrLst = append(dataStrLst, newInt64Val)

		recordLst = append(recordLst, &types.Record{
			Data:          dataStrLst,
			TableMetadata: tableOneMetadata,
		})
	}

	tm := &PixieToGrafanaTableMux{}
	tableMuxAcceptTableAndHandleRecord(t, tm, tableOneMetadata, recordLst)

	assert.Equal(t, 1, len(tm.pxTablePrinterLst))
	grafanaFrame := tm.pxTablePrinterLst[0].frame

	// Check the number of fields in frame.
	assert.Equal(t, 2, len(grafanaFrame.Fields))
	for idx, expectedRowValStr := range rowStringVals {
		val := grafanaFrame.Fields[0].At(idx).(string)
		assert.Equal(t, val, expectedRowValStr)
	}

	for idx, expectedRowValInt := range rowIntVals {
		val := grafanaFrame.Fields[1].At(idx).(int64)
		assert.Equal(t, val, expectedRowValInt)
	}
}

func TestTwoTables(t *testing.T) {
	tableOneMetadata := makeTableMetadata(vizierpb.TIME64NS)
	rowTimeVals := []time.Time{
		time.Now(),
		time.Now().Add(1 * time.Hour),
		time.Now().Add(-2 * time.Hour),
	}

	var recordLst []*types.Record
	for _, rowVal := range rowTimeVals {
		var dataStrLst []types.Datum

		newTime64NSVal := types.NewTime64NSValue(&tableOneMetadata.ColInfo[0])
		newTime64NSVal.ScanInt64(rowVal.UnixNano())
		dataStrLst = append(dataStrLst, newTime64NSVal)

		recordLst = append(recordLst, &types.Record{
			Data:          dataStrLst,
			TableMetadata: tableOneMetadata,
		})
	}

	tm := &PixieToGrafanaTableMux{}
	tableMuxAcceptTableAndHandleRecord(t, tm, tableOneMetadata, recordLst)

	tableTwoMetadata := makeTableMetadata(vizierpb.BOOLEAN)
	rowBoolVals := []bool{
		true,
		true,
		false,
	}

	var recordTwoLst []*types.Record
	for _, rowVal := range rowBoolVals {
		var dataStrLst []types.Datum

		newBoolVal := types.NewBooleanValue(&tableTwoMetadata.ColInfo[0])
		newBoolVal.ScanBool(rowVal)
		dataStrLst = append(dataStrLst, newBoolVal)

		recordTwoLst = append(recordTwoLst, &types.Record{
			Data:          dataStrLst,
			TableMetadata: tableTwoMetadata,
		})
	}

	tableMuxAcceptTableAndHandleRecord(t, tm, tableTwoMetadata, recordTwoLst)

	assert.Equal(t, 2, len(tm.pxTablePrinterLst))
	grafanaFrame := tm.pxTablePrinterLst[0].frame
	grafanSecondFrame := tm.pxTablePrinterLst[1].frame

	// Check the number of fields in frame.
	assert.Equal(t, 1, len(grafanaFrame.Fields))
	assert.Equal(t, 1, len(grafanSecondFrame.Fields))

	for idx, expectedRowValStr := range rowTimeVals {
		val := grafanaFrame.Fields[0].At(idx).(time.Time)
		assert.Equal(t, true, val.Equal(expectedRowValStr))
	}

	for idx, expectedRowVal := range rowBoolVals {
		val := grafanSecondFrame.Fields[0].At(idx).(bool)
		assert.Equal(t, val, expectedRowVal)
	}
}
