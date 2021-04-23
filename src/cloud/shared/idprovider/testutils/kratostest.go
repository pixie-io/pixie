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

package testutils

import (
	"context"
	"fmt"
	"io/ioutil"
	"net/http"
	"os"
	"path/filepath"
	"time"

	"github.com/cenkalti/backoff/v3"
	"github.com/gofrs/uuid"
	"github.com/ory/dockertest/v3"
	"github.com/ory/dockertest/v3/docker"
	log "github.com/sirupsen/logrus"
	"gopkg.in/yaml.v2"

	"px.dev/pixie/src/cloud/shared/idprovider"
	"px.dev/pixie/src/cloud/shared/idprovider/testutils/schema"
)

// KratosServer manages a kratos instance deployed to docker for testing.
type KratosServer struct {
	pool            *dockertest.Pool
	dockerDirectory string
	volumeName      string
	containerName   string
}

type kratosConfigStruct struct {
	Data struct {
		KratosConfig   string `yaml:"kratos.yml"`
		IdentitySchema string `yaml:"identity.schema.json"`
	} `yaml:"data"`
}

// NewKratosServer creates and initiates a new Kratos Testing Server.
func NewKratosServer() (*KratosServer, error) {
	pool, err := dockertest.NewPool("")
	if err != nil {
		return nil, fmt.Errorf("connect to docker failed: %w", err)
	}

	// Grab the TMPDIR directory created by Bazel. If the env var doesn't exist, it will default to "/tmp".
	kratosConfDir, err := ioutil.TempDir(os.Getenv("TEST_TMPDIR"), "example")
	if err != nil {
		return nil, fmt.Errorf("failed to create temp dir: %v", err)
	}
	err = os.Chmod(kratosConfDir, 0777)
	if err != nil {
		return nil, fmt.Errorf("failed to chmod: %v", err)
	}

	id, err := uuid.NewV4()
	if err != nil {
		return nil, fmt.Errorf("failed to create uuid: %v", err)
	}

	srv := &KratosServer{
		pool:            pool,
		dockerDirectory: kratosConfDir,
		volumeName:      id.String(),
		containerName:   id.String(),
	}

	// Load the kratos config.
	kratosConfig := &kratosConfigStruct{}
	kratosConfigYaml := schema.MustAsset("kratos_config.yaml")
	if err := yaml.Unmarshal(kratosConfigYaml, &kratosConfig); err != nil {
		return nil, fmt.Errorf("failed to unmarshal yaml: %v", err)
	}
	if err := srv.prepareDockerDirectory(kratosConfig); err != nil {
		return nil, err
	}

	return srv, nil
}

func (k *KratosServer) prepareDockerDirectory(cfg *kratosConfigStruct) error {
	kratosConfFile := "kratos.yml"
	identityFile := "identity.schema.json"

	if err := ioutil.WriteFile(filepath.Join(k.dockerDirectory, kratosConfFile), []byte(cfg.Data.KratosConfig), 0666); err != nil {
		return fmt.Errorf("\"%w\" while writing kratosConfFile", err)
	}

	if err := ioutil.WriteFile(filepath.Join(k.dockerDirectory, identityFile), []byte(cfg.Data.IdentitySchema), 0666); err != nil {
		return fmt.Errorf("\"%w\" while writing identitySchema", err)
	}

	return nil
}

// CleanUp removes all remaining state on the system if it exists.
func (k *KratosServer) CleanUp() {
	os.RemoveAll(k.dockerDirectory)
}

// Serve will start the main server and return a HydraKratosClient that can talk with that server.
func (k *KratosServer) Serve() (*idprovider.HydraKratosClient, func(), error) {
	resource, closeServer, err := k.runKratosContainer([]string{"serve"})
	if err != nil {
		return nil, nil, err
	}
	// Get logs in parallel.
	k.debugContainerLogs(resource)

	hostname := resource.Container.NetworkSettings.Gateway
	kratosPublicHost := fmt.Sprintf("http://%s:%s", hostname, resource.GetPort("4433/tcp"))
	kratosAdminHost := fmt.Sprintf("http://%s:%s", hostname, resource.GetPort("4434/tcp"))
	client, err := idprovider.NewHydraKratosClientFromConfig(&idprovider.HydraKratosConfig{
		KratosPublicHost: kratosPublicHost,
		KratosAdminHost:  kratosAdminHost,
		// Create an http client without tLS support.
		HTTPClient: &http.Client{},
	})
	if err != nil {
		return nil, nil, err
	}

	// Wait until the server is up and running before returning the client.
	connect := func() error {
		resp, err := http.Get(kratosPublicHost + "/health/alive")
		// We expect errors to be not nil when we first start.
		if err != nil {
			return err
		}
		if resp.StatusCode != http.StatusOK {
			return fmt.Errorf("expected OK, got response Code %d: %s", resp.StatusCode, resp.Status)
		}
		return nil
	}

	backOffOpts := backoff.NewExponentialBackOff()
	backOffOpts.InitialInterval = 250 * time.Millisecond
	backOffOpts.Multiplier = 2
	backOffOpts.MaxElapsedTime = 5 * time.Second
	err = backoff.Retry(connect, backOffOpts)
	if err != nil {
		closeServer()
		return nil, nil, err
	}

	return client, closeServer, err
}

func (k *KratosServer) runKratosContainer(cmdParts []string) (*dockertest.Resource, func(), error) {
	resource, err := k.pool.RunWithOptions(
		&dockertest.RunOptions{
			Repository: "oryd/kratos",
			Tag:        "v0.5.5-sqlite",
			Name:       k.containerName,
			Cmd:        append([]string{"-c", "/home/ory/kratos/kratos.yml"}, cmdParts...),
			Mounts: []string{
				fmt.Sprintf("%s:%s", k.dockerDirectory, "/home/ory/kratos"),
			},
			Env: []string{
				"DSN=memory",
				"SELFSERVICE_DEFAULT_BROWSER_RETURN_URL=https://work.withpixie.ai/",
				"IDENTITY_DEFAULT_SCHEMA_URL=file:///home/ory/kratos/identity.schema.json",
			},
			ExposedPorts: []string{"4433", "4434"},
		}, func(config *docker.HostConfig) {
			config.AutoRemove = true
			config.RestartPolicy = docker.RestartPolicy{Name: "no"}
			config.CPUCount = 1
			config.Memory = 32 * 1024 * 1024
			config.MemorySwap = 0
			config.MemorySwappiness = 0
		},
	)
	if err != nil {
		return nil, nil, fmt.Errorf("failed to build/run container: %v", err)
	}
	// Tell Docker to Stop the container in 60 seconds if it still exists.
	err = resource.Expire(60)
	if err != nil {
		return nil, nil, err
	}

	return resource, func() {
		if err := k.pool.Purge(resource); err != nil {
			log.WithError(err).Error("could not purge docker resource")
		}
	}, nil
}

func (k *KratosServer) debugContainerLogs(r *dockertest.Resource) {
	go func() {
		opts := docker.LogsOptions{
			Context:      context.Background(),
			Stderr:       true,
			Stdout:       true,
			Follow:       true,
			Timestamps:   true,
			RawTerminal:  true,
			Container:    r.Container.ID,
			OutputStream: log.StandardLogger().WriterLevel(log.DebugLevel),
		}

		err := k.pool.Client.Logs(opts)
		if err != nil {
			log.WithError(err).Errorf("Failed to get logs from resource")
		}
	}()
}
