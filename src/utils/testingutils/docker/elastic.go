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

package docker

import (
	"fmt"

	"github.com/olivere/elastic/v7"
	"github.com/ory/dockertest/v3"
	"github.com/ory/dockertest/v3/docker"
	log "github.com/sirupsen/logrus"
)

func connectElastic(esURL string, esUser string, esPass string) (*elastic.Client, error) {
	es, err := elastic.NewClient(elastic.SetURL(esURL),
		elastic.SetBasicAuth(esUser, esPass),
		elastic.SetSniff(false))
	if err != nil {
		return nil, err
	}
	return es, nil
}

// SetupElastic starts up an embedded elastic server on some free ports.
func SetupElastic() (*elastic.Client, func(), error) {
	cleanup := func() {}
	pool, err := dockertest.NewPool("")
	if err != nil {
		return nil, cleanup, fmt.Errorf("Could not connect to docker: %s", err)
	}

	esPass := "password"
	resource, err := pool.RunWithOptions(&dockertest.RunOptions{
		Repository: "elasticsearch",
		Tag:        "7.6.0",
		Env: []string{
			"discovery.type=single-node",
			fmt.Sprintf("ELASTIC_PASSWORD=%s", esPass),
			"xpack.security.http.ssl.enabled=false",
			"xpack.security.transport.ssl.enabled=false",
			"indices.lifecycle.poll_interval=5s",
			"path.data=/opt/elasticsearch/volatile/data",
			"path.logs=/opt/elasticsearch/volatile/logs",
			"ES_JAVA_OPTS=-Xms128m -Xmx128m -server",
			"ES_HEAP_SIZE=128m",
		},
	}, func(config *docker.HostConfig) {
		config.AutoRemove = true
		config.RestartPolicy = docker.RestartPolicy{Name: "no"}
		// Tmpfs is much faster than the default docker mounts.
		config.Mounts = []docker.HostMount{
			{
				Target: "/opt/elasticsearch/volatile/data",
				Type:   "tmpfs",
				TempfsOptions: &docker.TempfsOptions{
					SizeBytes: 100 * 1024 * 1024,
				},
			},
			{
				Target: "/opt/elasticsearch/volatile/logs",
				Type:   "tmpfs",
				TempfsOptions: &docker.TempfsOptions{
					SizeBytes: 100 * 1024 * 1024,
				},
			},
			{
				Target: "/tmp",
				Type:   "tmpfs",
				TempfsOptions: &docker.TempfsOptions{
					SizeBytes: 100 * 1024 * 1024,
				},
			},
		}
		config.CPUCount = 1
		config.Memory = 1024 * 1024 * 1024
		config.MemorySwap = 0
		config.MemorySwappiness = 0
	})
	if err != nil {
		return nil, cleanup, err
	}
	// Set a 5 minute expiration on resources.
	err = resource.Expire(300)
	if err != nil {
		return nil, cleanup, err
	}

	clientPort := resource.GetPort("9200/tcp")
	var client *elastic.Client
	err = pool.Retry(func() error {
		var err error
		client, err = connectElastic(fmt.Sprintf("http://%s:%s",
			resource.Container.NetworkSettings.Gateway, clientPort), "elastic", esPass)
		if err != nil {
			log.WithError(err).Errorf("Failed to connect to elasticsearch.")
		}
		return err
	})
	if err != nil {
		purgeErr := pool.Purge(resource)
		if purgeErr != nil {
			log.WithError(err).Error("Failed to purge pool")
		}
		return nil, cleanup, fmt.Errorf("Cannot start elasticsearch: %s", err)
	}

	log.Info("Successfully connected to elastic.")

	cleanup = func() {
		client.Stop()
		err = pool.Purge(resource)
		if err != nil {
			log.WithError(err).Error("Failed to purge pool")
		}
	}

	return client, cleanup, nil
}
