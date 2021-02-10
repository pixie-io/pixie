package main

import (
	"net/http"

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
)

func init() {
	pflag.String("database_key", "", "The encryption key to use for the database")
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
		log.WithError(err).Info("migrations failed: %s", err)
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
	healthz.RegisterDefaultChecks(mux)

	cfg := controllers.NewAuth0Config()
	a := controllers.NewAuth0Connector(cfg)
	if err := a.Init(); err != nil {
		log.WithError(err).Fatal("Failed to initialize Auth0")
	}

	env, err := authenv.NewWithDefaults()
	if err != nil {
		log.WithError(err).Fatal("Failed to initialize auth env")
	}

	db, dbKey := connectToPostgres()
	apiKeyMgr := apikey.New(db, dbKey)

	server, err := controllers.NewServer(env, a, apiKeyMgr)
	if err != nil {
		log.WithError(err).Fatal("Failed to initialize GRPC server funcs")
	}

	s := services.NewPLServer(env, mux)
	auth.RegisterAuthServiceServer(s.GRPCServer(), server)
	auth.RegisterAPIKeyServiceServer(s.GRPCServer(), apiKeyMgr)

	s.Start()
	s.StopOnInterrupt()
}
