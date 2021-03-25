package pg

import (
	"fmt"
	"time"

	// This is required to get the "pgx" driver.
	_ "github.com/jackc/pgx/stdlib"
	"github.com/jmoiron/sqlx"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
)

const retryAttempts = 5
const retryDelay = 1 * time.Second

func init() {
	pflag.Uint32("postgres_port", 5432, "The port for postgres database")
	pflag.String("postgres_hostname", "localhost", "The hostname for postgres database")
	pflag.String("postgres_db", "test", "The name of the database to use")
	pflag.String("postgres_username", "pl", "The username in the postgres database")
	pflag.String("postgres_password", "pl", "The password in the postgres database")
	pflag.Bool("postgres_ssl", false, "Enable ssl for postgres")
}

// DefaultDBURI returns the URI string for the default postgres instance based on flags/env vars.
func DefaultDBURI() string {
	dbPort := viper.GetInt32("postgres_port")
	dbHostname := viper.GetString("postgres_hostname")
	dbName := viper.Get("postgres_db")
	dbUsername := viper.GetString("postgres_username")
	dbPassword := viper.GetString("postgres_password")

	sslMode := "require"
	if !viper.GetBool("postgres_ssl") {
		sslMode = "disable"
	}

	dbURI := fmt.Sprintf("postgres://%s:%s@%s:%d/%s?sslmode=%s", dbUsername, dbPassword, dbHostname, dbPort, dbName, sslMode)
	return dbURI
}

// MustCreateDefaultPostgresDB creates a postgres DB instance.
func MustCreateDefaultPostgresDB() *sqlx.DB {
	dbURI := DefaultDBURI()
	log.WithField("dbURI", dbURI).Info("Setting up database")

	db, err := sqlx.Open("pgx", dbURI)
	if err != nil {
		log.WithError(err).Fatalf("failed to setup database connection")
	}

	db.SetMaxIdleConns(5)
	db.SetConnMaxLifetime(2 * time.Minute)
	db.SetMaxOpenConns(10)

	return db
}

// MustConnectDefaultPostgresDB tries to connect to default postgres database as defined by the environment
// variables/flags.
func MustConnectDefaultPostgresDB() *sqlx.DB {
	db := MustCreateDefaultPostgresDB()
	var err error
	for i := retryAttempts; i >= 0; i-- {
		err = db.Ping()
		if err == nil {
			log.Info("Connected to Postgres")
			break
		}
		if i > 0 {
			log.WithError(err).Error("failed to connect to DB, retrying")
			time.Sleep(retryDelay)
		}
	}

	if err != nil {
		log.WithError(err).Fatalf("failed to initialized database connection")
	}
	return db
}
