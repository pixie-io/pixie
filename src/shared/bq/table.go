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

package bq

import (
	"context"
	"fmt"
	"net/http"

	"cloud.google.com/go/bigquery"
	"google.golang.org/api/googleapi"
)

// Table is a wrapper around bigquery's Table which also stores the bigquery Schema.
// Storing the bigquery.Schema with the bigquery.Table is useful because the Schema is required for some bigquery operations
// (for example, bigquery.StructSaver based puts).
type Table struct {
	Schema bigquery.Schema
	Table  *bigquery.Table
}

// NewTableForStruct returns a new bigquery Table wrapper storing rows of type `s`, creating the bigquery table and dataset if necessary.
func NewTableForStruct(project string, datasetName string, datasetLoc string, tableName string, timePartitioning *bigquery.TimePartitioning, s interface{}) (*Table, error) {
	client, err := bigquery.NewClient(context.Background(), project)
	if err != nil {
		return nil, err
	}
	defer client.Close()

	dataset := client.Dataset(datasetName)
	err = dataset.Create(context.Background(), &bigquery.DatasetMetadata{Location: datasetLoc})
	if err != nil {
		apiError, ok := err.(*googleapi.Error)
		if !ok {
			return nil, fmt.Errorf("unexpected error creating dataset: %v", err)
		}
		// StatusConflict indicates that this dataset already exists.
		// If so, we can carry along. Else we hit something else unexpected.
		if apiError.Code != http.StatusConflict {
			return nil, err
		}
	}

	schema, err := bigquery.InferSchema(s)
	if err != nil {
		return nil, err
	}
	table := dataset.Table(tableName)

	// Check if the table already exists, if so, just return.
	_, err = table.Metadata(context.Background())
	if err == nil {
		return &Table{
			Schema: schema,
			Table:  table,
		}, nil
	}

	// Table needs to be created.
	err = table.Create(context.Background(), &bigquery.TableMetadata{
		Schema:           schema,
		TimePartitioning: timePartitioning,
	})
	if err != nil {
		return nil, err
	}
	return &Table{
		Schema: schema,
		Table:  table,
	}, nil
}

// Inserter returns the underlying bigquery.Table's Inserter.
func (t *Table) Inserter() *bigquery.Inserter {
	return t.Table.Inserter()
}
