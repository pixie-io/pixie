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
	"net/http"
	_ "net/http/pprof"

	bindata "github.com/golang-migrate/migrate/source/go_bindata"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"

	"px.dev/pixie/src/cloud/cron_script/cronscriptpb"
	"px.dev/pixie/src/cloud/plugin/controllers"
	"px.dev/pixie/src/cloud/plugin/pluginpb"
	"px.dev/pixie/src/cloud/plugin/schema"
	"px.dev/pixie/src/cloud/shared/pgmigrate"
	"px.dev/pixie/src/shared/services"
	"px.dev/pixie/src/shared/services/env"
	"px.dev/pixie/src/shared/services/healthz"
	"px.dev/pixie/src/shared/services/pg"
	"px.dev/pixie/src/shared/services/server"
)

func init() {
	pflag.String("cron_script_service", "cron-script-service.plc.svc.cluster.local:50700", "The cronscript service url (load balancer/list is ok)")
}

// NewCronScriptServiceClient creates a new cron script service RPC client stub.
func NewCronScriptServiceClient() (cronscriptpb.CronScriptServiceClient, error) {
	dialOpts, err := services.GetGRPCClientDialOpts()
	if err != nil {
		return nil, err
	}

	csChannel, err := grpc.Dial(viper.GetString("cron_script_service"), dialOpts...)
	if err != nil {
		return nil, err
	}

	return cronscriptpb.NewCronScriptServiceClient(csChannel), nil
}

func main() {
	services.SetupService("plugin-service", 50600)
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.SetupServiceLogging()

	mux := http.NewServeMux()
	// This handles all the pprof endpoints.
	mux.Handle("/debug/", http.DefaultServeMux)
	healthz.RegisterDefaultChecks(mux)

	db := pg.MustConnectDefaultPostgresDB()
	err := pgmigrate.PerformMigrationsUsingBindata(db, "plugin_service_migrations",
		bindata.Resource(schema.AssetNames(), schema.Asset))
	if err != nil {
		log.WithError(err).Fatal("Failed to apply migrations")
	}

	dbKey := viper.GetString("database_key")
	if dbKey == "" {
		log.Fatal("Database encryption key is required")
	}

	s := server.NewPLServer(env.New(viper.GetString("domain_name")), mux)

	csClient, err := NewCronScriptServiceClient()
	if err != nil {
		log.Fatal("Failed to start cronscript client")
	}
	c := controllers.New(db, dbKey, csClient)

	pluginpb.RegisterPluginServiceServer(s.GRPCServer(), c)
	pluginpb.RegisterDataRetentionPluginServiceServer(s.GRPCServer(), c)

	s.Start()
	s.StopOnInterrupt()
}
