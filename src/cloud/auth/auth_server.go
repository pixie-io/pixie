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
	"github.com/jmoiron/sqlx"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"

	"px.dev/pixie/src/cloud/auth/apikey"
	"px.dev/pixie/src/cloud/auth/authenv"
	"px.dev/pixie/src/cloud/auth/authpb"
	"px.dev/pixie/src/cloud/auth/controllers"
	"px.dev/pixie/src/cloud/auth/schema"
	"px.dev/pixie/src/cloud/shared/pgmigrate"
	"px.dev/pixie/src/shared/services"
	"px.dev/pixie/src/shared/services/healthz"
	"px.dev/pixie/src/shared/services/pg"
	"px.dev/pixie/src/shared/services/server"
)

func init() {
	pflag.String("database_key", "", "The encryption key to use for the database")
	pflag.String("oauth_provider", "auth0", "The auth provider to use. Supported values are 'oidc', 'auth0' or 'hydra'")
	pflag.String("domain_name", "dev.withpixie.dev", "The domain name of Pixie Cloud")
}

func connectToPostgres() (*sqlx.DB, string) {
	db := pg.MustConnectDefaultPostgresDB()
	err := pgmigrate.PerformMigrationsUsingBindata(db, "auth_service_migrations",
		bindata.Resource(schema.AssetNames(), schema.Asset))
	if err != nil {
		log.WithError(err).Fatal("Failed to apply migrations")
	}

	dbKey := viper.GetString("database_key")
	if dbKey == "" {
		log.Fatal("Database encryption key is required")
	}

	return db, dbKey
}

func main() {
	services.SetupService("auth-service", 50100)
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.SetupServiceLogging()

	flush := services.InitDefaultSentry()
	defer flush()

	mux := http.NewServeMux()
	// This handles all the pprof endpoints.
	mux.Handle("/debug/", http.DefaultServeMux)
	healthz.RegisterDefaultChecks(mux)

	var err error
	var a controllers.AuthProvider

	authProvider := viper.GetString("oauth_provider")

	switch authProvider {
	case "oidc":
		a, err = controllers.NewOIDCConnector()
		if err != nil {
			log.WithError(err).Fatal("Failed to initialize OIDC connector")
		}
	case "auth0":
		a, err = controllers.NewAuth0Connector(controllers.NewAuth0Config())
		if err != nil {
			log.WithError(err).Fatal("Failed to initialize Auth0")
		}
	case "hydra":
		a, err = controllers.NewHydraKratosConnector()
		if err != nil {
			log.WithError(err).Fatal("Failed to initialize hydraKratosConnector")
		}
	default:
		log.Fatalf("Cannot initialize authProvider '%s'. Only 'auth0', 'oidc', and 'hydra' are supported.", authProvider)
	}

	env, err := authenv.NewWithDefaults()
	if err != nil {
		log.WithError(err).Fatal("Failed to initialize auth env")
	}

	db, dbKey := connectToPostgres()
	apiKeyMgr := apikey.New(db, dbKey)

	svr, err := controllers.NewServer(env, a, apiKeyMgr)
	if err != nil {
		log.WithError(err).Fatal("Failed to initialize GRPC server funcs")
	}

	s := server.NewPLServer(env, mux)
	authpb.RegisterAuthServiceServer(s.GRPCServer(), svr)
	authpb.RegisterAPIKeyServiceServer(s.GRPCServer(), apiKeyMgr)

	s.Start()
	s.StopOnInterrupt()
}
