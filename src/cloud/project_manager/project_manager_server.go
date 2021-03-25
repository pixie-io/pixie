package main

import (
	"net/http"
	_ "net/http/pprof"

	log "github.com/sirupsen/logrus"

	controllers "pixielabs.ai/pixielabs/src/cloud/project_manager/controller"
	"pixielabs.ai/pixielabs/src/cloud/project_manager/datastore"
	"pixielabs.ai/pixielabs/src/cloud/project_manager/projectmanagerpb"
	"pixielabs.ai/pixielabs/src/cloud/project_manager/schema"
	"pixielabs.ai/pixielabs/src/cloud/shared/pgmigrate"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/shared/services/env"
	"pixielabs.ai/pixielabs/src/shared/services/healthz"
	"pixielabs.ai/pixielabs/src/shared/services/pg"
	"pixielabs.ai/pixielabs/src/shared/services/server"
)

func main() {
	services.SetupService("project-manager-service", 50300)
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.SetupServiceLogging()

	mux := http.NewServeMux()
	// This handles all the pprof endpoints.
	mux.Handle("/debug/", http.DefaultServeMux)
	healthz.RegisterDefaultChecks(mux)

	db := pg.MustConnectDefaultPostgresDB()
	err := pgmigrate.PerformMigrationsUsingBindata(db, "project_manager_service_migrations", &pgmigrate.SchemaAssetFetcher{
		AssetNames: schema.AssetNames,
		Asset:      schema.Asset,
	})
	if err != nil {
		log.WithError(err).Fatal("Failed to apply migrations")
	}

	datastore, err := datastore.NewDatastore(db)
	if err != nil {
		log.WithError(err).Fatalf("Failed to initialize datastore")
	}

	svr := controllers.NewServer(datastore)
	s := server.NewPLServer(env.New(), mux)
	projectmanagerpb.RegisterProjectManagerServiceServer(s.GRPCServer(), svr)

	s.Start()
	s.StopOnInterrupt()
}
