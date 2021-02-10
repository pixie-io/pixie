package main

import (
	"net/http"

	"github.com/golang-migrate/migrate"
	"github.com/golang-migrate/migrate/database/postgres"
	bindata "github.com/golang-migrate/migrate/source/go_bindata"
	"pixielabs.ai/pixielabs/src/cloud/profile/controller"
	"pixielabs.ai/pixielabs/src/cloud/profile/datastore"
	"pixielabs.ai/pixielabs/src/cloud/profile/profileenv"
	profile "pixielabs.ai/pixielabs/src/cloud/profile/profilepb"
	"pixielabs.ai/pixielabs/src/cloud/profile/schema"
	"pixielabs.ai/pixielabs/src/shared/services/pg"

	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/shared/services/healthz"
)

func main() {
	services.SetupService("profile-service", 51500)
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.SetupServiceLogging()

	mux := http.NewServeMux()
	healthz.RegisterDefaultChecks(mux)

	db := pg.MustConnectDefaultPostgresDB()

	// TODO(zasgar): Pull out this migration code into a util. Just leaving it here for now for testing.
	driver, err := postgres.WithInstance(db.DB, &postgres.Config{
		MigrationsTable: "profile_service_migrations",
	})

	sc := bindata.Resource(schema.AssetNames(), func(name string) (bytes []byte, e error) {
		return schema.Asset(name)
	})

	d, err := bindata.WithInstance(sc)

	mg, err := migrate.NewWithInstance(
		"go-bindata",
		d, "postgres", driver)

	if err = mg.Up(); err != nil {
		log.WithError(err).Info("migrations failed: %s", err)
	}

	datastore := datastore.NewDatastore(db)

	env, err := profileenv.NewWithDefaults()
	if err != nil {
		log.WithError(err).Fatal("Failed to set up profileenv")
	}
	server := controller.NewServer(env, datastore, datastore)

	s := services.NewPLServer(env, mux)
	profile.RegisterProfileServiceServer(s.GRPCServer(), server)
	s.Start()
	s.StopOnInterrupt()
}
