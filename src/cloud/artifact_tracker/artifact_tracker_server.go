package main

import (
	"context"
	"io/ioutil"
	"net/http"

	"github.com/spf13/viper"

	"github.com/jmoiron/sqlx"
	"github.com/spf13/pflag"

	"golang.org/x/oauth2/jwt"

	"golang.org/x/oauth2/google"
	"pixielabs.ai/pixielabs/src/shared/services/pg"

	"cloud.google.com/go/storage"
	"github.com/golang-migrate/migrate"
	"github.com/golang-migrate/migrate/database/postgres"
	bindata "github.com/golang-migrate/migrate/source/go_bindata"
	"github.com/googleapis/google-cloud-go-testing/storage/stiface"
	log "github.com/sirupsen/logrus"
	"google.golang.org/api/option"
	"pixielabs.ai/pixielabs/src/cloud/artifact_tracker/artifacttrackerenv"
	atpb "pixielabs.ai/pixielabs/src/cloud/artifact_tracker/artifacttrackerpb"
	"pixielabs.ai/pixielabs/src/cloud/artifact_tracker/controller"
	"pixielabs.ai/pixielabs/src/cloud/artifact_tracker/schema"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/shared/services/healthz"
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

	// TODO(zasgar): Pull out this migration code into a util. Just leaving it here for now for testing.
	driver, err := postgres.WithInstance(db.DB, &postgres.Config{
		MigrationsTable: "artifacts_tracker_service_migrations",
	})

	sc := bindata.Resource(schema.AssetNames(), func(name string) (bytes []byte, e error) {
		return schema.Asset(name)
	})

	d, err := bindata.WithInstance(sc)
	if err != nil {
		log.Fatalln(err)
	}

	mg, err := migrate.NewWithInstance(
		"go-bindata",
		d, "postgres", driver)

	if err = mg.Up(); err != nil {
		log.WithError(err).Info("migrations failed: %s", err)
	}
	return db
}

func main() {
	services.SetupService("artifact-tracker-service", 50750)
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.SetupServiceLogging()

	mux := http.NewServeMux()
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
	server := controller.NewServer(db, stiface.AdaptClient(client), bucket, saCfg)
	s := services.NewPLServer(env, mux)
	atpb.RegisterArtifactTrackerServer(s.GRPCServer(), server)
	s.Start()
	s.StopOnInterrupt()
}
