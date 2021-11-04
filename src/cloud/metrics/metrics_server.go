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
	"google.golang.org/api/option"

	"px.dev/pixie/src/cloud/metrics/controller"
	"px.dev/pixie/src/shared/services"
	"px.dev/pixie/src/shared/services/env"
	"px.dev/pixie/src/shared/services/healthz"
	"px.dev/pixie/src/shared/services/msgbus"
	"px.dev/pixie/src/shared/services/server"
)

func init() {
	pflag.String("bq_project", "", "The BigQuery project to write metrics to.")
	pflag.String("bq_sa_key_path", "", "The service account for the BigQuery instance that should be used.")
}

func main() {
	services.SetupService("metrics-service", 50800)
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.SetupServiceLogging()

	mux := http.NewServeMux()
	// This handles all the pprof endpoints.
	mux.Handle("/debug/", http.DefaultServeMux)
	healthz.RegisterDefaultChecks(mux)

	// Connect to NATS.
	nc := msgbus.MustConnectNATS()

	// Connect to BigQuery.
	var client *bigquery.Client
	var err error

	if viper.GetString("bq_sa_key_path") != "" {
		client, err = bigquery.NewClient(context.Background(), viper.GetString("bq_project"), option.WithCredentialsFile(viper.GetString("bq_sa_key_path")))
		if err != nil {
			log.WithError(err).Fatal("Could not start up BigQuery client for metrics server")
		}
		defer client.Close()
		_ = controller.NewServer(nc, client)
	} else {
		log.Info("No BigQuery instance configured, no metrics will be sent")
	}

	s := server.NewPLServer(env.New(viper.GetString("domain_name")), mux)
	s.Start()
	s.StopOnInterrupt()
}
