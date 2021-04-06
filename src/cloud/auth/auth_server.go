package main

import (
	"net/http"
	_ "net/http/pprof"

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
	"pixielabs.ai/pixielabs/src/cloud/shared/pgmigrate"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/shared/services/healthz"
	"pixielabs.ai/pixielabs/src/shared/services/pg"
	"pixielabs.ai/pixielabs/src/shared/services/server"
)

func init() {
	pflag.String("database_key", "", "The encryption key to use for the database")
	pflag.String("oauth_provider", "auth0", "The auth provider to user. Currently support 'auth0' or 'hydra'")
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

	mux := http.NewServeMux()
	// This handles all the pprof endpoints.
	mux.Handle("/debug/", http.DefaultServeMux)
	healthz.RegisterDefaultChecks(mux)

	var err error
	var a controllers.AuthProvider

	authProvider := viper.GetString("oauth_provider")

	switch authProvider {
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
