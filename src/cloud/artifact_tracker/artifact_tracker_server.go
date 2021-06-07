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
	"io/ioutil"
	"net/http"
	_ "net/http/pprof"

	"cloud.google.com/go/storage"
	bindata "github.com/golang-migrate/migrate/source/go_bindata"
	"github.com/googleapis/google-cloud-go-testing/storage/stiface"
	"github.com/jmoiron/sqlx"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"golang.org/x/oauth2/google"
	"golang.org/x/oauth2/jwt"
	"google.golang.org/api/option"

	"px.dev/pixie/src/cloud/artifact_tracker/artifacttrackerenv"
	atpb "px.dev/pixie/src/cloud/artifact_tracker/artifacttrackerpb"
	"px.dev/pixie/src/cloud/artifact_tracker/controller"
	"px.dev/pixie/src/cloud/artifact_tracker/schema"
	"px.dev/pixie/src/cloud/shared/pgmigrate"
	"px.dev/pixie/src/shared/services"
	"px.dev/pixie/src/shared/services/healthz"
	"px.dev/pixie/src/shared/services/pg"
	"px.dev/pixie/src/shared/services/server"
)

func init() {
	pflag.String("artifact_bucket", "pl-artifacts", "The name of the artifact bucket.")
	pflag.String("release_artifact_bucket", "pl-artifacts", "The name of the artifact bucket containing official releases.")
	pflag.String("sa_key_path", "/creds/service_account.json", "The path to the service account JSON file.")
	pflag.String("vizier_version", "", "If specified, the db will not be queried. The only vizier version is assumed to be the one specified.")
	pflag.String("cli_version", "", "If specified, the db will not be queried. The only CLI version is assumed to be the one specified.")
	pflag.String("operator_version", "", "If specified, the db will not be queried. The only operator version is assumed to be the one specified.")
}

func loadServiceAccountConfig() *jwt.Config {
	saKeyFile := viper.GetString("sa_key_path")
	saKey, err := ioutil.ReadFile(saKeyFile)

	if err != nil {
		return nil
	}

	saCfg, err := google.JWTConfigFromJSON(saKey)
	if err != nil {
		return nil
	}
	return saCfg
}

func mustLoadDB() *sqlx.DB {
	db := pg.MustConnectDefaultPostgresDB()

	err := pgmigrate.PerformMigrationsUsingBindata(db, "artifacts_tracker_service_migrations",
		bindata.Resource(schema.AssetNames(), schema.Asset))
	if err != nil {
		log.WithError(err).Fatal("Failed to apply migrations")
	}
	return db
}

func main() {
	services.SetupService("artifact-tracker-service", 50750)
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.SetupServiceLogging()

	mux := http.NewServeMux()
	// This handles all the pprof endpoints.
	mux.Handle("/debug/", http.DefaultServeMux)
	healthz.RegisterDefaultChecks(mux)

	saCfg := loadServiceAccountConfig()

	ctx := context.Background()
	var client *storage.Client
	var err error

	if saCfg != nil {
		client, err = storage.NewClient(ctx, option.WithCredentialsFile(viper.GetString("sa_key_path")))
	} else {
		client, err = storage.NewClient(ctx, option.WithoutAuthentication())
	}
	if err != nil {
		log.WithError(err).Fatal("Failed to initialize GCS client.")
	}

	env := artifacttrackerenv.New()

	db := mustLoadDB()
	bucket := viper.GetString("artifact_bucket")
	releaseBucket := viper.GetString("release_artifact_bucket")
	svr := controller.NewServer(db, stiface.AdaptClient(client), bucket, releaseBucket, saCfg)

	serverOpts := &server.GRPCServerOptions{
		DisableAuth: map[string]bool{
			"/px.services.ArtifactTracker/GetArtifactList": true,
			"/px.services.ArtifactTracker/GetDownloadLink": true,
			"/pl.services.ArtifactTracker/GetArtifactList": true,
			"/pl.services.ArtifactTracker/GetDownloadLink": true,
		},
	}

	s := server.NewPLServerWithOptions(env, mux, serverOpts)
	atpb.RegisterArtifactTrackerServer(s.GRPCServer(), svr)
	s.Start()
	s.StopOnInterrupt()
}
