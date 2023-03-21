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

	"px.dev/pixie/src/cloud/metrics/controllers"
	"px.dev/pixie/src/cloud/shared/messages"
	"px.dev/pixie/src/cloud/shared/vzshard"
	"px.dev/pixie/src/shared/services"
	"px.dev/pixie/src/shared/services/env"
	"px.dev/pixie/src/shared/services/healthz"
	"px.dev/pixie/src/shared/services/metrics"
	"px.dev/pixie/src/shared/services/msgbus"
	"px.dev/pixie/src/shared/services/server"
)

var natsErrorCounter *messages.NatsErrorCounter

func init() {
	pflag.String("bq_project", "", "The BigQuery project to write metrics to.")
	pflag.String("bq_sa_key_path", "", "The service account for the BigQuery instance that should be used.")

	pflag.String("bq_dataset", "vizier_metrics", "The BigQuery dataset to write metrics to.")
	pflag.String("bq_dataset_loc", "", "The location for the BigQuery dataset. Used during creation.")

	natsErrorCounter = messages.NewNatsErrorCounter()
}

func main() {
	vzshard.SetupFlags()
	services.SetupService("metrics-service", 50800)
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.SetupServiceLogging()

	mux := http.NewServeMux()
	// This handles all the pprof endpoints.
	mux.Handle("/debug/", http.DefaultServeMux)
	healthz.RegisterDefaultChecks(mux)
	metrics.MustRegisterMetricsHandler(mux)

	// Connect to NATS.
	nc := msgbus.MustConnectNATS()

	nc.SetErrorHandler(natsErrorCounter.HandleNatsError)

	// Connect to BigQuery.
	var client *bigquery.Client
	var err error

	if viper.GetString("bq_sa_key_path") != "" && viper.GetString("bq_project") != "" {
		client, err = bigquery.NewClient(context.Background(), viper.GetString("bq_project"), option.WithCredentialsFile(viper.GetString("bq_sa_key_path")))
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
		mc := controllers.NewServer(nc, dataset)
		mc.Start()
		defer mc.Stop()
	} else {
		log.Info("No BigQuery instance configured, no metrics will be sent")
	}

	s := server.NewPLServer(env.New(viper.GetString("domain_name")), mux)
	s.Start()
	s.StopOnInterrupt()
}
