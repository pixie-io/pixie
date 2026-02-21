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
	"bufio"
	"fmt"
	"os"
	"regexp"
	"time"

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

// selfContainerID returns the current Docker container ID by parsing
// /proc/self/mountinfo. Docker bind-mounts /etc/hostname from
// /var/lib/docker/containers/<id>/hostname, exposing the container ID.
// Returns empty string if not running inside a Docker container.
func selfContainerID() string {
	f, err := os.Open("/proc/self/mountinfo")
	if err != nil {
		return ""
	}
	defer f.Close()

	re := regexp.MustCompile(`/containers/([a-f0-9]{64})/hostname`)
	scanner := bufio.NewScanner(f)
	for scanner.Scan() {
		if m := re.FindStringSubmatch(scanner.Text()); m != nil {
			return m[1]
		}
	}
	return ""
}

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
	// Set a 15 minute expiration on resources (extended for debugging).
	err = resource.Expire(900)
	if err != nil {
		return nil, nil, err
	}

	// When running inside a container (e.g. CI), the postgres container is on
	// a different Docker network and we can't reach it via host port mapping.
	// Detect this and connect postgres to our network instead.
	pgHost := resource.Container.NetworkSettings.Gateway
	pgPort := resource.GetPort("5432/tcp")
	selfID := selfContainerID()
	log.Infof("selfContainerID: %q", selfID)
	if selfID != "" {
		selfContainer, err := pool.Client.InspectContainer(selfID)
		if err != nil {
			return nil, nil, fmt.Errorf("failed to inspect self container %s: %w", selfID, err)
		}
		for netName, net := range selfContainer.NetworkSettings.Networks {
			if netName == "host" {
				continue
			}
			err := pool.Client.ConnectNetwork(net.NetworkID, docker.NetworkConnectionOptions{
				Container: resource.Container.ID,
			})
			if err != nil {
				return nil, nil, fmt.Errorf("failed to connect postgres to network %s: %w", netName, err)
			}
			// Re-inspect to get the postgres container's IP on our network.
			updated, err := pool.Client.InspectContainer(resource.Container.ID)
			if err != nil {
				return nil, nil, fmt.Errorf("failed to re-inspect postgres container: %w", err)
			}
			resource.Container = updated
			if pgNet, ok := updated.NetworkSettings.Networks[netName]; ok {
				pgHost = pgNet.IPAddress
				pgPort = "5432"
				log.Infof("pgHost set to %s:%s via network %s", pgHost, pgPort, netName)
			}
			break
		}
	}
	if pgHost == "" {
		pgHost = "localhost"
	}
	viper.Set("postgres_port", pgPort)
	viper.Set("postgres_hostname", pgHost)
	viper.Set("postgres_db", dbName)
	viper.Set("postgres_username", "postgres")
	viper.Set("postgres_password", "secret")

	pool.MaxWait = 10 * time.Minute
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
