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

package pgtest

import (
	"fmt"

	"github.com/golang-migrate/migrate"
	"github.com/golang-migrate/migrate/database/postgres"
	bindata "github.com/golang-migrate/migrate/source/go_bindata"
	"github.com/jmoiron/sqlx"
	"github.com/ory/dockertest/v3"
	"github.com/ory/dockertest/v3/docker"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"

	"px.dev/pixie/src/shared/services/pg"
)

// SetupTestDB sets up a test database instance and applies migrations.
func SetupTestDB(schemaSource *bindata.AssetSource) (*sqlx.DB, func(), error) {
	var db *sqlx.DB

	pool, err := dockertest.NewPool("")
	if err != nil {
		return nil, nil, fmt.Errorf("connect to docker failed: %w", err)
	}

	const dbName = "testdb"
	resource, err := pool.RunWithOptions(
		&dockertest.RunOptions{
			Repository: "postgres",
			Tag:        "13.3",
			Env:        []string{"POSTGRES_PASSWORD=secret", "POSTGRES_DB=" + dbName},
		}, func(config *docker.HostConfig) {
			config.AutoRemove = true
			config.RestartPolicy = docker.RestartPolicy{Name: "no"}
			config.Mounts = []docker.HostMount{
				{
					Target: "/var/lib/postgresql/data",
					Type:   "tmpfs",
					TempfsOptions: &docker.TempfsOptions{
						SizeBytes: 100 * 1024 * 1024,
					},
				},
			}
			config.CPUCount = 1
			config.Memory = 512 * 1024 * 1024
			config.MemorySwap = 0
			config.MemorySwappiness = 0
		},
	)
	if err != nil {
		return nil, nil, fmt.Errorf("Failed to run docker pool: %w", err)
	}
	// Set a 5 minute expiration on resources.
	err = resource.Expire(300)
	if err != nil {
		return nil, nil, err
	}

	viper.Set("postgres_port", resource.GetPort("5432/tcp"))
	viper.Set("postgres_hostname", resource.Container.NetworkSettings.Gateway)
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

	return db, func() {
		if db != nil {
			db.Close()
		}

		if err := pool.Purge(resource); err != nil {
			log.WithError(err).Error("could not purge docker resource")
		}
	}, nil
}
