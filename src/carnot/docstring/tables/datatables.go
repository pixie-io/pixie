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
	"encoding/json"
	"fmt"
	"os"
	"sort"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"

	"px.dev/pixie/src/api/go/pxapi"
	"px.dev/pixie/src/api/go/pxapi/errdefs"
	"px.dev/pixie/src/api/go/pxapi/muxes"
	"px.dev/pixie/src/api/go/pxapi/types"
	"px.dev/pixie/src/shared/services"
)

var (
	pxl = `
import px
px.display(px.GetTables(), 'table_desc')
px.display(px.GetSchemas(), 'table_schemas')
`
)

func init() {
	pflag.String("cluster_id", "", "The cluster_id of the cluster to query")
	pflag.String("api_key", "", "The api-key used to authenticate")
	pflag.String("output_json", "", "The file to write output JSON to")
}

func main() {
	services.PostFlagSetupAndParse()

	apiSchema := pxAPISchema{clusterID: viper.GetString("cluster_id"), apiKey: viper.GetString("api_key")}
	dataSchema := apiSchema.GetSchema()

	// Marshal out the full structure.
	outb, err := json.MarshalIndent(dataSchema, "", " ")
	if err != nil {
		log.Fatal(err)
	}

	// Write out the file.
	outputF, err := os.Create(viper.GetString("output_json"))
	if err != nil {
		log.Fatal(err)
	}
	defer outputF.Close()

	_, err = outputF.Write(outb)
	if err != nil {
		log.Fatal(err)
	}
}

// pxAPISchema holds the information necessary to create a new pxapi client and connect to a cluster.
type pxAPISchema struct {
	clusterID string
	apiKey    string
}

func (p *pxAPISchema) GetSchema() DataSchema {
	ctx := context.Background()
	client, err := pxapi.NewClient(ctx, pxapi.WithAPIKey(viper.GetString("api_key")))
	if err != nil {
		panic(err)
	}
	clusterID := viper.GetString("cluster_id")
	vz, err := client.NewVizierClient(ctx, clusterID)
	if err != nil {
		panic(err)
	}

	// Log Vizier Version
	clusterInfo, err := client.GetVizierInfo(ctx, clusterID)
	if err != nil {
		panic(err)
	}
	fmt.Printf("Running on Cluster: %s\n", viper.GetString("cluster_id"))
	fmt.Printf("Vizier Version: %s\n", clusterInfo.Version)

	// Execute the PxL query.
	tm := muxes.NewRegexTableMux()
	th := &TableRecordHandler{}
	err = tm.RegisterHandlerForPattern("table_desc", func(metadata types.TableMetadata) (pxapi.TableRecordHandler, error) {
		return th, nil
	})
	if err != nil {
		panic(err)
	}

	ch := &ColRecordHandler{}
	err = tm.RegisterHandlerForPattern("table_schemas", func(metadata types.TableMetadata) (pxapi.TableRecordHandler, error) {
		return ch, nil
	})
	if err != nil {
		panic(err)
	}
	resultSet, err := vz.ExecuteScript(ctx, pxl, tm)
	if err != nil {
		panic(err)
	}

	defer resultSet.Close()
	if err := resultSet.Stream(); err != nil {
		if errdefs.IsCompilationError(err) {
			fmt.Printf("Got compiler error: \n %s\n", err.Error())
		} else {
			fmt.Printf("Got error : %+v, while streaming\n", err)
		}
	}

	// Process the query output.
	// Add the column info to the table struct.
	tables := th.tables
	cols := ch.columns
	for i := range tables {
		if val, ok := cols[tables[i].Name]; ok {
			tables[i].Cols = val
		}
	}

	// Sort the tables in alphabetical order.
	sort.Slice(tables[:], func(i, j int) bool {
		return tables[i].Name < tables[j].Name
	})

	return DataSchema{DatatableDocs: tables}
}

// DataSchema is the top-level json object that the docs sites recognize.
type DataSchema struct {
	DatatableDocs []*DataTable `json:"datatableDocs"`
}

// DataTable holds information about each Pixie data source table.
type DataTable struct {
	Name string             `json:"name"`
	Desc string             `json:"desc"`
	Cols []*DataTableColumn `json:"cols"`
}

// DataTableColumn holds information about the columns in the data source tables.
type DataTableColumn struct {
	Name    string `json:"name"`
	Type    string `json:"type"`
	Pattern string `json:"pattern"`
	Desc    string `json:"desc"`
}

// TableRecordHandler interface to processes the PxL script output table record-wise.
type TableRecordHandler struct {
	tables []*DataTable
}

// HandleInit is called when the table metadata is available.
func (t *TableRecordHandler) HandleInit(ctx context.Context, metadata types.TableMetadata) error {
	t.tables = []*DataTable{}
	return nil
}

// HandleRecord is called for each record of the table.
func (t *TableRecordHandler) HandleRecord(ctx context.Context, r *types.Record) error {
	name := r.GetDatum("table_name").String()
	desc := r.GetDatum("table_desc").String()
	table := &DataTable{Name: name, Desc: desc, Cols: []*DataTableColumn{}}
	t.tables = append(t.tables, table)
	return nil
}

// HandleDone is called when all data has been streamed.
func (t *TableRecordHandler) HandleDone(ctx context.Context) error {
	return nil
}

// ColRecordHandler implements the TableRecordHandler interface to processes the PxL script output table record-wise.
type ColRecordHandler struct {
	columns map[string][]*DataTableColumn
}

// HandleInit is called when the table metadata is available.
func (t *ColRecordHandler) HandleInit(ctx context.Context, metadata types.TableMetadata) error {
	t.columns = make(map[string][]*DataTableColumn)
	return nil
}

// HandleRecord is called for each record of the table.
func (t *ColRecordHandler) HandleRecord(ctx context.Context, r *types.Record) error {
	table := r.GetDatum("table_name").String()
	colName := r.GetDatum("column_name").String()
	colType := r.GetDatum("column_type").String()
	patternType := r.GetDatum("pattern_type").String()
	colDesc := r.GetDatum("column_desc").String()
	column := &DataTableColumn{Name: colName, Type: colType, Pattern: patternType, Desc: colDesc}
	t.columns[table] = append(t.columns[table], column)
	return nil
}

// HandleDone is called when all data has been streamed.
func (t *ColRecordHandler) HandleDone(ctx context.Context) error {
	return nil
}
