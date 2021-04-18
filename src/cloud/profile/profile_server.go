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

	"px.dev/pixie/src/cloud/profile/controller"
	"px.dev/pixie/src/cloud/profile/controller/idmanager"
	"px.dev/pixie/src/cloud/profile/datastore"
	"px.dev/pixie/src/cloud/profile/profileenv"
	profile "px.dev/pixie/src/cloud/profile/profilepb"
	"px.dev/pixie/src/cloud/profile/schema"
	"px.dev/pixie/src/cloud/shared/idprovider"
	"px.dev/pixie/src/cloud/shared/pgmigrate"
	"px.dev/pixie/src/shared/services"
	"px.dev/pixie/src/shared/services/healthz"
	"px.dev/pixie/src/shared/services/pg"
	"px.dev/pixie/src/shared/services/server"
)

func init() {
	pflag.String("oauth_provider", "auth0", "The auth provider to user. Currently support 'auth0' or 'hydra'")
}

func main() {
	services.SetupService("profile-service", 51500)
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.SetupServiceLogging()

	mux := http.NewServeMux()
	// This handles all the pprof endpoints.
	mux.Handle("/debug/", http.DefaultServeMux)
	healthz.RegisterDefaultChecks(mux)

	db := pg.MustConnectDefaultPostgresDB()
	err := pgmigrate.PerformMigrationsUsingBindata(db, "profile_service_migrations",
		bindata.Resource(schema.AssetNames(), schema.Asset))
	if err != nil {
		log.WithError(err).Fatal("Failed to apply migrations")
	}

	datastore := datastore.NewDatastore(db)
	env, err := profileenv.NewWithDefaults()
	if err != nil {
		log.WithError(err).Fatal("Failed to set up profileenv")
	}

	var mgr idmanager.Manager
	switch viper.GetString("oauth_provider") {
	case "auth0":
		mgr, err = controller.NewAuth0Manager()
		if err != nil {
			log.WithError(err).Fatal("Failed to initialize Auth0")
		}
	case "hydra":
		mgr, err = idprovider.NewHydraKratosClient()
		if err != nil {
			log.WithError(err).Fatal("Failed to initialize hydraKratosConnector")
		}
	default:
		log.Fatalf("Cannot initialize authProvider '%s'. Only 'auth0' and 'hydra' are supported.", viper.GetString("oauth_provider"))
	}
	svr := controller.NewServer(env, datastore, datastore, mgr)

	s := server.NewPLServer(env, mux)
	profile.RegisterProfileServiceServer(s.GRPCServer(), svr)
	s.Start()
	s.StopOnInterrupt()
}
