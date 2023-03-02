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
	"errors"
	"net/http"
	_ "net/http/pprof"

	"px.dev/pixie/src/cloud/shared/messages"

	bindata "github.com/golang-migrate/migrate/source/go_bindata"
	"github.com/nats-io/nats.go"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"

	"px.dev/pixie/src/cloud/artifact_tracker/artifacttrackerpb"
	"px.dev/pixie/src/cloud/shared/pgmigrate"
	"px.dev/pixie/src/cloud/shared/vzshard"
	"px.dev/pixie/src/cloud/vzmgr/controllers"
	"px.dev/pixie/src/cloud/vzmgr/deployment"
	"px.dev/pixie/src/cloud/vzmgr/deploymentkey"
	"px.dev/pixie/src/cloud/vzmgr/schema"
	"px.dev/pixie/src/cloud/vzmgr/vzmgrpb"
	"px.dev/pixie/src/shared/services"
	"px.dev/pixie/src/shared/services/env"
	"px.dev/pixie/src/shared/services/healthz"
	"px.dev/pixie/src/shared/services/metrics"
	"px.dev/pixie/src/shared/services/msgbus"
	"px.dev/pixie/src/shared/services/pg"
	"px.dev/pixie/src/shared/services/server"
)

var natsErrorCounter *messages.NatsErrorCounter

func init() {
	pflag.String("database_key", "", "The encryption key to use for the database")
	pflag.String("domain_name", "dev.withpixie.dev", "The domain name of Pixie Cloud")

	natsErrorCounter = messages.NewNatsErrorCounter()
}

// NewArtifactTrackerServiceClient creates a new artifact tracker RPC client stub.
func NewArtifactTrackerServiceClient() (artifacttrackerpb.ArtifactTrackerClient, error) {
	dialOpts, err := services.GetGRPCClientDialOpts()
	if err != nil {
		return nil, err
	}

	atChannel, err := grpc.Dial(viper.GetString("artifact_tracker_service"), dialOpts...)
	if err != nil {
		return nil, err
	}

	return artifacttrackerpb.NewArtifactTrackerClient(atChannel), nil
}

type readinessCheck struct {
	err error
}

func (r *readinessCheck) Name() string {
	return "md_reader_readiness"
}

func (r *readinessCheck) Check() error {
	return r.err
}

func mustSetupNATSAndJetStream() (*nats.Conn, msgbus.Streamer) {
	nc := msgbus.MustConnectNATS()
	js := msgbus.MustConnectJetStream(nc)
	strmr, err := msgbus.NewJetStreamStreamer(nc, js, msgbus.V2CDurableStream)
	if err != nil {
		log.WithError(err).Fatal("Could not start JetStream streamer")
	}

	nc.SetErrorHandler(natsErrorCounter.HandleNatsError)
	return nc, strmr
}

func main() {
	services.SetupService("vzmgr-service", 51800)
	vzshard.SetupFlags()
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.SetupServiceLogging()

	flush := services.InitDefaultSentry()
	defer flush()

	mux := http.NewServeMux()
	// This handles all the pprof endpoints.
	mux.Handle("/debug/", http.DefaultServeMux)
	healthz.RegisterDefaultChecks(mux)
	rc := &readinessCheck{
		err: errors.New("metadata reader is not yet ready"),
	}
	healthz.InstallPathHandler(mux, "/readyz", rc)
	metrics.MustRegisterMetricsHandler(mux)

	s := server.NewPLServer(env.New(viper.GetString("domain_name")), mux)

	db := pg.MustConnectDefaultPostgresDB()
	// We have 256 * 2 different sharded goroutines running to handle requests.
	// Match the same number of allowed db connections.
	db.SetMaxOpenConns(512)
	db.SetMaxIdleConns(128)
	err := pgmigrate.PerformMigrationsUsingBindata(db, "vzmgr_service_migrations",
		bindata.Resource(schema.AssetNames(), schema.Asset))
	if err != nil {
		log.WithError(err).Fatal("Failed to apply migrations")
	}

	dbKey := viper.GetString("database_key")
	if dbKey == "" {
		log.Fatal("Database encryption key is required")
	}

	// Connect to NATS.
	nc, strmr := mustSetupNATSAndJetStream()
	defer nc.Close()

	at, err := NewArtifactTrackerServiceClient()
	if err != nil {
		log.Fatal("Could not connect to artifact tracker")
	}

	updater, err := controllers.NewUpdater(db, at, nc)
	if err != nil {
		log.WithError(err).Fatal("Could not start vizier updater")
	}
	go updater.ProcessUpdateQueue()
	defer updater.Stop()

	c := controllers.New(db, dbKey, nc, updater)
	dks := deploymentkey.New(db, dbKey)
	ds := deployment.New(dks, c)

	sm := controllers.NewStatusMonitor(db)
	defer sm.Stop()
	vzmgrpb.RegisterVZMgrServiceServer(s.GRPCServer(), c)
	vzmgrpb.RegisterVZDeploymentKeyServiceServer(s.GRPCServer(), dks)
	vzmgrpb.RegisterVZDeploymentServiceServer(s.GRPCServer(), ds)

	var mdr *controllers.MetadataReader
	go func() {
		mdr, err = controllers.NewMetadataReader(db, strmr, nc)
		if err != nil {
			log.WithError(err).Fatal("Could not start metadata listener")
		}
		rc.err = nil
	}()

	defer func() {
		if mdr != nil {
			mdr.Stop()
		}
	}()

	s.Start()
	s.StopOnInterrupt()
}
