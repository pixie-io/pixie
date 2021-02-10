package pgtest

import (
	"testing"

	"pixielabs.ai/pixielabs/src/shared/services/pg"

	"github.com/golang-migrate/migrate"
	"github.com/golang-migrate/migrate/database/postgres"
	bindata "github.com/golang-migrate/migrate/source/go_bindata"
	"github.com/jmoiron/sqlx"
	"github.com/ory/dockertest/v3"
	"github.com/ory/dockertest/v3/docker"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"
	"github.com/stretchr/testify/require"
)

// SetupTestDB sets up a test database instance and applies migrations.
func SetupTestDB(t *testing.T, schemaSource *bindata.AssetSource) (*sqlx.DB, func()) {
	// TODO(zasgar): refactor into a helper utility.
	var db *sqlx.DB

	pool, err := dockertest.NewPool("")
	if err != nil {
		t.Fatalf("Could not connect to docker: %s", err)
	}

	const dbName = "testdb"
	resource, err := pool.RunWithOptions(
		&dockertest.RunOptions{
			Repository: "postgres",
			Tag:        "11.1",
			Env:        []string{"POSTGRES_PASSWORD=secret", "POSTGRES_DB=" + dbName},
		}, func(config *docker.HostConfig) {
			config.AutoRemove = true
			config.RestartPolicy = docker.RestartPolicy{Name: "no"}
		},
	)
	if err != nil {
		t.Fatal(err)
	}
	// Set a 5 minute expiration on resources.
	resource.Expire(300)
	require.Nil(t, err)

	viper.Set("postgres_port", resource.GetPort("5432/tcp"))
	viper.Set("postgres_hostname", "localhost")
	viper.Set("postgres_db", dbName)
	viper.Set("postgres_username", "postgres")
	viper.Set("postgres_password", "secret")

	if err = pool.Retry(func() error {
		log.Info("trying to connect")
		db = pg.MustCreateDefaultPostgresDB()
		return db.Ping()
	}); err != nil {
		t.Fatalf("Could not connect to docker: %s", err)
	}

	driver, err := postgres.WithInstance(db.DB, &postgres.Config{})
	require.Nil(t, err)

	if schemaSource != nil {
		d, err := bindata.WithInstance(schemaSource)
		require.Nil(t, err)
		mg, err := migrate.NewWithInstance(
			"go-bindata",
			d, "postgres", driver)
		require.Nil(t, err)

		if err = mg.Up(); err != nil {
			t.Fatalf("migrations failed: %s", err)
		}
	}

	return db, func() {
		if db != nil {
			db.Close()
		}

		if err := pool.Purge(resource); err != nil {
			t.Fatalf("Could not purge resource: %s", err)
		}
	}
}
