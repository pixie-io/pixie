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

	"pixielabs.ai/pixielabs/src/cloud/artifact_tracker/artifacttrackerenv"
	atpb "pixielabs.ai/pixielabs/src/cloud/artifact_tracker/artifacttrackerpb"
	"pixielabs.ai/pixielabs/src/cloud/artifact_tracker/controller"
	"pixielabs.ai/pixielabs/src/cloud/artifact_tracker/schema"
	"pixielabs.ai/pixielabs/src/cloud/shared/pgmigrate"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/shared/services/healthz"
	"pixielabs.ai/pixielabs/src/shared/services/pg"
	"pixielabs.ai/pixielabs/src/shared/services/server"
)

func init() {
	pflag.String("artifact_bucket", "pl-artifacts", "The name of the artifact bucket.")
	pflag.String("sa_key_path", "/creds/service_account.json", "The path to the service account JSON file.")
}

func mustLoadServiceAccountConfig() *jwt.Config {
	saKeyFile := viper.GetString("sa_key_path")
	saKey, err := ioutil.ReadFile(saKeyFile)

	if err != nil {
		log.Fatalln(err)
	}

	saCfg, err := google.JWTConfigFromJSON(saKey)
	if err != nil {
		log.Fatalln(err)
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

	ctx := context.Background()
	client, err := storage.NewClient(ctx, option.WithCredentialsFile(viper.GetString("sa_key_path")))
	if err != nil {
		log.WithError(err).Fatal("Failed to initialize GCS client.")
	}

	env := artifacttrackerenv.New()

	db := mustLoadDB()
	saCfg := mustLoadServiceAccountConfig()
	bucket := viper.GetString("artifact_bucket")
	svr := controller.NewServer(db, stiface.AdaptClient(client), bucket, saCfg)
	s := server.NewPLServer(env, mux)
	atpb.RegisterArtifactTrackerServer(s.GRPCServer(), svr)
	s.Start()
	s.StopOnInterrupt()
}
