package main

import (
	"net/http"
	_ "net/http/pprof"

	bindata "github.com/golang-migrate/migrate/source/go_bindata"
	log "github.com/sirupsen/logrus"

	"px.dev/pixie/src/cloud/profile/controller"
	"px.dev/pixie/src/cloud/profile/datastore"
	"px.dev/pixie/src/cloud/profile/profileenv"
	profile "px.dev/pixie/src/cloud/profile/profilepb"
	"px.dev/pixie/src/cloud/profile/schema"
	"px.dev/pixie/src/cloud/shared/pgmigrate"
	"px.dev/pixie/src/shared/services"
	"px.dev/pixie/src/shared/services/healthz"
	"px.dev/pixie/src/shared/services/pg"
	"px.dev/pixie/src/shared/services/server"
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
	svr := controller.NewServer(env, datastore, datastore)

	s := server.NewPLServer(env, mux)
	profile.RegisterProfileServiceServer(s.GRPCServer(), svr)
	s.Start()
	s.StopOnInterrupt()
}
