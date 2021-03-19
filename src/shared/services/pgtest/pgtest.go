package pgtest

import (
	"fmt"
	"testing"

	"github.com/golang-migrate/migrate"
	"github.com/golang-migrate/migrate/database/postgres"
	bindata "github.com/golang-migrate/migrate/source/go_bindata"
	"github.com/jmoiron/sqlx"
	"github.com/ory/dockertest/v3"
	"github.com/ory/dockertest/v3/docker"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"
	"github.com/stretchr/testify/require"

	"pixielabs.ai/pixielabs/src/shared/services/pg"
)

// SetupTestDB sets up a test database instance and applies migrations.
// Deprecated: Replaced with SetupTestDBNew, once all invocations are removed this function will be removed.
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

// SetupTestDBNew sets up a test database instance and applies migrations.
func SetupTestDBNew(schemaSource *bindata.AssetSource) (*sqlx.DB, func() error, error) {
	// TODO(zasgar): refactor into a helper utility.
	var db *sqlx.DB

	pool, err := dockertest.NewPool("")
	if err != nil {
		return nil, nil, fmt.Errorf("connect to docker failed: %w", err)
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
		return nil, nil, fmt.Errorf("Failed to run docker pool: %w", err)
	}
	// Set a 5 minute expiration on resources.
	resource.Expire(300)

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
		return nil, nil, fmt.Errorf("failed to create postgres on docker: %w", err)
	}

	driver, err := postgres.WithInstance(db.DB, &postgres.Config{})
	if err != nil {
		return nil, nil, fmt.Errorf("failed to get postgres driver: %w", err)
	}

	if schemaSource != nil {
		d, err := bindata.WithInstance(schemaSource)
		if err != nil {
			return nil, nil, fmt.Errorf("failed to load schema: %w", err)
		}
		mg, err := migrate.NewWithInstance(
			"go-bindata",
			d, "postgres", driver)
		if err != nil {
			return nil, nil, fmt.Errorf("failed to load migrations: %w", err)
		}

		if err = mg.Up(); err != nil {
			return nil, nil, fmt.Errorf("migrations failed: %w", err)
		}
	}

	return db, func() error {
		if db != nil {
			db.Close()
		}

		if err := pool.Purge(resource); err != nil {
			return fmt.Errorf("could not purge docker resource: %w", err)
		}
		return nil
	}, nil
}
