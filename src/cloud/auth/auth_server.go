package main

import (
	"net/http"
	_ "net/http/pprof"

	"github.com/golang-migrate/migrate"
	"github.com/golang-migrate/migrate/database/postgres"
	bindata "github.com/golang-migrate/migrate/source/go_bindata"
	"github.com/jmoiron/sqlx"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"

	"pixielabs.ai/pixielabs/src/cloud/auth/apikey"
	"pixielabs.ai/pixielabs/src/cloud/auth/authenv"
	"pixielabs.ai/pixielabs/src/cloud/auth/controllers"
	auth "pixielabs.ai/pixielabs/src/cloud/auth/proto"
	"pixielabs.ai/pixielabs/src/cloud/auth/schema"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/shared/services/healthz"
	"pixielabs.ai/pixielabs/src/shared/services/pg"
	"pixielabs.ai/pixielabs/src/shared/services/server"
)

func init() {
	pflag.String("database_key", "", "The encryption key to use for the database")
	pflag.String("oauth_provider", "auth0", "The auth provider to user. Currently support 'auth0' or 'hydra'")
}

func connectToPostgres() (*sqlx.DB, string) {
	db := pg.MustConnectDefaultPostgresDB()

	driver, err := postgres.WithInstance(db.DB, &postgres.Config{
		MigrationsTable: "auth_service_migrations",
	})

	sc := bindata.Resource(schema.AssetNames(), func(name string) (bytes []byte, e error) {
		return schema.Asset(name)
	})

	d, err := bindata.WithInstance(sc)

	mg, err := migrate.NewWithInstance(
		"go-bindata",
		d, "postgres", driver)

	if err = mg.Up(); err != nil {
		log.WithError(err).Infof("migrations failed: %s", err)
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

	mux := http.NewServeMux()
	// This handles all the pprof endpoints.
	mux.Handle("/debug/", http.DefaultServeMux)
	healthz.RegisterDefaultChecks(mux)

	var err error
	var a controllers.AuthProvider

	authProvider := viper.GetString("oauth_provider")

	if authProvider == "auth0" {
		a, err = controllers.NewAuth0Connector(controllers.NewAuth0Config())
		if err != nil {
			log.WithError(err).Fatal("Failed to initialize Auth0")
		}
	} else if authProvider == "hydra" {
		a, err = controllers.NewHydraKratosConnector()
		if err != nil {
			log.WithError(err).Fatal("Failed to initialize hydraKratosConnector")
		}
	} else {
		log.Fatalf("Cannot initialize authProvider '%s'. Only 'auth0' and 'hydra' are supported.", authProvider)
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
	auth.RegisterAuthServiceServer(s.GRPCServer(), svr)
	auth.RegisterAPIKeyServiceServer(s.GRPCServer(), apiKeyMgr)

	s.Start()
	s.StopOnInterrupt()
}
