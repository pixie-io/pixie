/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package pg

import (
	"fmt"
	"net"
	"net/url"
	"time"

	// This is required to get the "pgx" driver.
	_ "github.com/jackc/pgx/stdlib"
	"github.com/jmoiron/sqlx"
	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/collectors"
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
	dbName := viper.GetString("postgres_db")
	dbUsername := viper.GetString("postgres_username")
	dbPassword := viper.GetString("postgres_password")

	sslMode := "require"
	if !viper.GetBool("postgres_ssl") {
		sslMode = "disable"
	}

	v := url.Values{}
	v.Set("sslmode", sslMode)

	u := url.URL{
		Scheme:   "postgres",
		Host:     net.JoinHostPort(dbHostname, fmt.Sprintf("%d", dbPort)),
		User:     url.UserPassword(dbUsername, dbPassword),
		Path:     dbName,
		RawQuery: v.Encode(),
	}

	return u.String()
}

// MustCreateDefaultPostgresDB creates a postgres DB instance.
func MustCreateDefaultPostgresDB() *sqlx.DB {
	dbURI := DefaultDBURI()
	log.WithField("db_hostname", viper.GetString("postgres_hostname")).
		WithField("db_port", viper.GetInt32("postgres_port")).
		WithField("db_name", viper.GetString("postgres_db")).
		WithField("db_username", viper.GetString("postgres_username")).
		Info("Setting up database")

	db, err := sqlx.Open("pgx", dbURI)
	if err != nil {
		log.WithError(err).Fatalf("failed to setup database connection")
	}

	db.SetMaxIdleConns(5)
	db.SetConnMaxLifetime(30 * time.Minute)
	db.SetMaxOpenConns(10)

	// It's possible we already registered a prometheus collector with multiple DB connections.
	_ = prometheus.Register(
		collectors.NewDBStatsCollector(db.DB, viper.GetString("postgres_db")))
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
