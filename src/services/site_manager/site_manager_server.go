package main

import (
	"net/http"

	"github.com/golang-migrate/migrate"
	"github.com/golang-migrate/migrate/database/postgres"
	bindata "github.com/golang-migrate/migrate/source/go_bindata"
	log "github.com/sirupsen/logrus"
	controllers "pixielabs.ai/pixielabs/src/services/site_manager/controller"
	"pixielabs.ai/pixielabs/src/services/site_manager/datastore"
	"pixielabs.ai/pixielabs/src/services/site_manager/schema"
	"pixielabs.ai/pixielabs/src/services/site_manager/sitemanagerenv"
	"pixielabs.ai/pixielabs/src/services/site_manager/sitemanagerpb"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/shared/services/healthz"
	"pixielabs.ai/pixielabs/src/shared/services/pg"
)

func main() {
	log.WithField("service", "site-manager-service").Info("Starting service")

	services.SetupService("site-manager-service", 50300)
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.SetupServiceLogging()

	mux := http.NewServeMux()
	healthz.RegisterDefaultChecks(mux)

	env, err := sitemanagerenv.New()
	if err != nil {
		log.WithError(err).Fatal("Failed to initialize env")
	}

	db := pg.MustConnectDefaultPostgresDB()

	// TODO(zasgar): Pull out this migration code into a util. Just leaving it here for now for testing.
	driver, err := postgres.WithInstance(db.DB, &postgres.Config{})

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

	datastore, err := datastore.NewDatastore(db)
	if err != nil {
		log.WithError(err).Fatalf("Failed to initialize datastore")
	}

	server, err := controllers.NewServer(env, datastore)
	if err != nil {
		log.WithError(err).Fatal("Failed to initialize GRPC server funcs")
	}

	s := services.NewPLServer(env, mux)
	sitemanagerpb.RegisterSiteManagerServiceServer(s.GRPCServer(), server)

	s.Start()
	s.StopOnInterrupt()
}
