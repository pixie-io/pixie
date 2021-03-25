package main

import (
	"net/http"
	_ "net/http/pprof"

	log "github.com/sirupsen/logrus"

	"pixielabs.ai/pixielabs/src/cloud/profile/controller"
	"pixielabs.ai/pixielabs/src/cloud/profile/datastore"
	"pixielabs.ai/pixielabs/src/cloud/profile/profileenv"
	profile "pixielabs.ai/pixielabs/src/cloud/profile/profilepb"
	"pixielabs.ai/pixielabs/src/cloud/profile/schema"
	"pixielabs.ai/pixielabs/src/cloud/shared/pgmigrate"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/shared/services/healthz"
	"pixielabs.ai/pixielabs/src/shared/services/pg"
	"pixielabs.ai/pixielabs/src/shared/services/server"
)

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
	err := pgmigrate.PerformMigrationsUsingBindata(db, "profile_service_migrations", &pgmigrate.SchemaAssetFetcher{
		AssetNames: schema.AssetNames,
		Asset:      schema.Asset,
	})
	if err != nil {
		log.WithError(err).Fatal("Failed to apply migrations")
	}

	datastore := datastore.NewDatastore(db)
	env, err := profileenv.NewWithDefaults()
	if err != nil {
		log.WithError(err).Fatal("Failed to set up profileenv")
	}
	svr := controller.NewServer(env, datastore, datastore)

	s := server.NewPLServer(env, mux)
	profile.RegisterProfileServiceServer(s.GRPCServer(), svr)
	s.Start()
	s.StopOnInterrupt()
}
