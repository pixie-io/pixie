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

	"px.dev/pixie/src/cloud/shared/pgmigrate"
	"px.dev/pixie/src/e2e_test/perf_tool/backend/builder/builderenv"
	"px.dev/pixie/src/e2e_test/perf_tool/backend/builder/builderpb"
	"px.dev/pixie/src/e2e_test/perf_tool/backend/builder/controllers"
	"px.dev/pixie/src/e2e_test/perf_tool/backend/builder/datastore"
	"px.dev/pixie/src/e2e_test/perf_tool/backend/builder/schema"
	"px.dev/pixie/src/shared/services"
	"px.dev/pixie/src/shared/services/healthz"
	"px.dev/pixie/src/shared/services/pg"
	"px.dev/pixie/src/shared/services/server"
)

func main() {
	services.SetupService("builder-service", 50300)
	services.PostFlagSetupAndParse()
	services.SetupServiceLogging()

	mux := http.NewServeMux()
	healthz.RegisterDefaultChecks(mux)

	db := pg.MustConnectDefaultPostgresDB()
	err := pgmigrate.PerformMigrationsUsingBindata(db, "builder_service_migrations",
		bindata.Resource(schema.AssetNames(), schema.Asset))
	if err != nil {
		log.WithError(err).Fatal("Failed to apply migrations")
	}
	d := datastore.NewDatastore(db)

	notifyClient, err := builderenv.NewBuildNotificationServiceClient()
	if err != nil {
		log.WithError(err).Fatal("failed to connect to coordinator service")
	}

	_ = notifyClient

	svr := controllers.NewServer(d)
	s := server.NewPLServerWithOptions(nil, mux, &server.GRPCServerOptions{
		DisableMiddleware: true,
	})
	builderpb.RegisterBuilderServiceServer(s.GRPCServer(), svr)
	builderpb.RegisterBuilderWorkerCoordinationServiceServer(s.GRPCServer(), svr)

	s.Start()
	s.StopOnInterrupt()
}
