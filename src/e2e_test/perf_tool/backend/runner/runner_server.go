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
	"net/http"
	_ "net/http/pprof"

	"cloud.google.com/go/bigquery"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/api/googleapi"
	"google.golang.org/api/option"

	"px.dev/pixie/src/e2e_test/perf_tool/backend/runner/controllers"
	"px.dev/pixie/src/e2e_test/perf_tool/backend/runner/runnerpb"
	"px.dev/pixie/src/shared/services"
	"px.dev/pixie/src/shared/services/healthz"
	"px.dev/pixie/src/shared/services/server"
)

func init() {
	pflag.String("bq_project", "", "The BigQuery project to write to.")
	pflag.String("bq_sa_key_path", "", "The service account for the BigQuery instance that should be used.")
	pflag.String("bq_dataset", "perf_tool", "The BigQuery dataset to write to.")
	pflag.String("bq_dataset_loc", "", "The location for the BigQuery dataset. Used during creation.")
	pflag.String("bq_results_table", "results", "The name of the BigQuery table to write results to.")
	pflag.String("bq_specs_table", "specs", "The name of the BigQuery table to write experiment specs to.")
}

func mustSetupBigQuery() *bigquery.Dataset {
	if viper.GetString("bq_sa_key_path") == "" || viper.GetString("bq_project") != "" {
		log.Fatal("no bigquery configuration specified")
	}
	client, err := bigquery.NewClient(context.Background(), viper.GetString("bq_project"), option.WithCredentialsFile(viper.GetString("bq_sa_key_path")))
	if err != nil {
		log.WithError(err).Fatal("Could not start up BigQuery client for metrics server")
	}
	defer client.Close()

	dsName := viper.GetString("bq_dataset")
	if dsName == "" {
		log.WithError(err).Fatal("Missing a BigQuery dataset name.")
	}

	dsLoc := viper.GetString("bq_dataset_loc")

	dataset := client.Dataset(dsName)
	err = dataset.Create(context.Background(), &bigquery.DatasetMetadata{Location: dsLoc})
	apiError, ok := err.(*googleapi.Error)
	if !ok {
		log.WithError(err).Fatal("Problem with BigQuery dataset")
	}
	// StatusConflict indicates that this dataset already exists.
	// If so, we can carry along. Else we hit something else unexpected.
	if apiError.Code != http.StatusConflict {
		log.WithError(err).Fatal("Problem with BigQuery dataset")
	}
	return dataset
}

func mustCreateResultsTable(dataset *bigquery.Dataset) *bigquery.Table {
	if viper.GetString("bq_results_table") == "" {
		log.Fatal("must specify bq_results_table")
	}
	schema, err := bigquery.InferSchema(controllers.ResultRow{})
	if err != nil {
		log.WithError(err).Fatal("failed to infer row schema")
	}
	table := dataset.Table(viper.GetString("bq_results_table"))

	// Check if the table already exists, if so, just return.
	_, err = table.Metadata(context.Background())
	if err == nil {
		return table
	}

	// Table needs to be created.
	err = table.Create(context.Background(), &bigquery.TableMetadata{
		Schema: schema,
		TimePartitioning: &bigquery.TimePartitioning{
			Type:  bigquery.DayPartitioningType,
			Field: "timestamp",
		},
	})
	if err != nil {
		log.WithError(err).Fatal("failed to create results table")
	}
	return table
}

func mustCreateSpecsTable(dataset *bigquery.Dataset) *bigquery.Table {
	if viper.GetString("bq_specs_table") == "" {
		log.Fatal("must specify bq_specs_table")
	}
	schema, err := bigquery.InferSchema(controllers.SpecRow{})
	if err != nil {
		log.WithError(err).Fatal("failed to infer row schema")
	}
	table := dataset.Table(viper.GetString("bq_specs_table"))

	// Check if the table already exists, if so, just return.
	_, err = table.Metadata(context.Background())
	if err == nil {
		return table
	}

	// Table needs to be created.
	err = table.Create(context.Background(), &bigquery.TableMetadata{
		Schema: schema,
	})
	if err != nil {
		log.WithError(err).Fatal("failed to create specs table")
	}
	return table
}

func main() {
	services.SetupService("runner-service", 50200)
	services.PostFlagSetupAndParse()
	services.SetupServiceLogging()

	mux := http.NewServeMux()
	healthz.RegisterDefaultChecks(mux)

	bqDataset := mustSetupBigQuery()
	bqResults := mustCreateResultsTable(bqDataset)
	bqSpecs := mustCreateSpecsTable(bqDataset)

	svr := controllers.NewServer(bqResults, bqSpecs)
	s := server.NewPLServerWithOptions(nil, mux, &server.GRPCServerOptions{
		DisableMiddleware: true,
	})
	runnerpb.RegisterRunnerServiceServer(s.GRPCServer(), svr)

	s.Start()
	s.StopOnInterrupt()
}
