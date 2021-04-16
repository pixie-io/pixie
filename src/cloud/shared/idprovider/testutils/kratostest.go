package testutils

import (
	"context"
	"fmt"
	"html/template"
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
	dockerFile      string
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
	// Clean up the volume first in case any have been around. Will not fail if no volume exists.
	if err := srv.removeVolumeIfExists(); err != nil {
		return nil, err
	}

	// Run the migration server.
	if err := srv.migrate(time.Minute); err != nil {
		return nil, fmt.Errorf("\"%w\" while migrating the server", err)
	}

	return srv, nil
}

const dockerfileText = `
FROM oryd/kratos:v0.5.5-sqlite

COPY {{.KratosYml}} /etc/config/kratos/kratos.yml
COPY {{.IdentityJSON}} /etc/config/kratos/identity.schema.json
`

var dockerfileTmpl = template.Must(template.New("dockerfile").Parse(dockerfileText))

func (k *KratosServer) prepareDockerDirectory(cfg *kratosConfigStruct) error {
	kratosConfFile := "kratos.yml"
	identityFile := "identity.schema.json"

	if err := ioutil.WriteFile(filepath.Join(k.dockerDirectory, kratosConfFile), []byte(cfg.Data.KratosConfig), 0666); err != nil {
		return fmt.Errorf("\"%w\" while writing kratosConfFile", err)
	}

	if err := ioutil.WriteFile(filepath.Join(k.dockerDirectory, identityFile), []byte(cfg.Data.IdentitySchema), 0666); err != nil {
		return fmt.Errorf("\"%w\" while writing identitySchema", err)
	}

	k.dockerFile = filepath.Join(k.dockerDirectory, "Dockerfile")
	f, err := os.Create(k.dockerFile)
	if err != nil {
		return fmt.Errorf("\"%w\" while creating Dockerfile", err)
	}

	err = dockerfileTmpl.Execute(f, struct {
		KratosYml    string
		IdentityJSON string
	}{
		KratosYml:    kratosConfFile,
		IdentityJSON: identityFile,
	})
	if err != nil {
		return fmt.Errorf("\"%w\" while writing dockerfile", err)
	}

	return nil
}

func (k *KratosServer) removeVolumeIfExists() error {
	// remove volume.
	err := k.pool.Client.RemoveVolumeWithOptions(docker.RemoveVolumeOptions{Name: k.volumeName})
	// Not an error, the volume was cleaned up
	if err == docker.ErrNoSuchVolume {
		return nil
	}
	return err
}

func (k *KratosServer) killContainerIfExists() error {
	err := k.pool.Client.KillContainer(docker.KillContainerOptions{ID: k.containerName})
	if err == nil {
		return nil
	}
	// Ignore errors where the container has already stopped running.
	if _, ok := err.(*docker.NoSuchContainer); ok {
		return nil
	} else if _, ok := err.(*docker.ContainerNotRunning); ok {
		return nil
	}

	return err
}

// CleanUp removes all remaining state on the system if it exists.
func (k *KratosServer) CleanUp() {
	os.RemoveAll(k.dockerDirectory)
	err := k.removeVolumeIfExists()
	if err != nil {
		// Special case where the docker container is still alive.
		if err == docker.ErrVolumeInUse {
			err := k.killContainerIfExists()
			if err != nil {
				log.WithError(err).Fatal("Failed to kill container")
			}
			return
		}
		log.WithError(err).Fatal("Failed to clean up volume")
	}
}

func (k *KratosServer) migrate(cleanupTimeout time.Duration) error {
	resource, closeContainer, err := k.runKratosContainer([]string{"migrate", "sql", "-e", "--yes"})
	if err != nil {
		return err
	}

	// If logrus is in debug mode, will tail the container logs into logrus.
	k.debugContainerLogs(resource)

	// Check the Kratos.
	ch := make(chan struct{})
	errorCh := make(chan error)
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	go func() {
		_, err := k.pool.Client.WaitContainerWithContext(resource.Container.ID, ctx)
		if err != nil {
			errorCh <- err
			return
		}

		ch <- struct{}{}
	}()

	// Handle waiting for the migration.
	timeout := time.NewTicker(cleanupTimeout)
	defer timeout.Stop()
	for {
		select {
		// Error while waiting.
		case err := <-errorCh:
			closeContainer()
			close(ch)
			close(errorCh)
			return err
		// Timeout
		case <-timeout.C:
			closeContainer()
			close(ch)
			close(errorCh)
			return fmt.Errorf("migration timed out")
		// Success
		case <-ch:
			return nil
		}
	}
}

// Serve will start the main server and return a HydraKratosClient that can talk with that server.
func (k *KratosServer) Serve() (*idprovider.HydraKratosClient, func(), error) {
	resource, closeServer, err := k.runKratosContainer([]string{"serve"})
	if err != nil {
		return nil, nil, err
	}
	// Get logs in parallel.
	k.debugContainerLogs(resource)

	kratosPublicHost := fmt.Sprintf("http://localhost:%s", resource.GetPort("4433/tcp"))
	kratosAdminHost := fmt.Sprintf("http://localhost:%s", resource.GetPort("4434/tcp"))
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
	// Need to use the absolute path for the dockerfile
	dockerfileAbsPath, err := filepath.Abs(k.dockerFile)
	if err != nil {
		return nil, nil, fmt.Errorf("\"%w\" while getting abs", err)
	}

	resource, err := k.pool.BuildAndRunWithOptions(dockerfileAbsPath,
		&dockertest.RunOptions{
			Name: k.containerName,
			Cmd:  append([]string{"-c", "/etc/config/kratos/kratos.yml"}, cmdParts...),
			Env: []string{
				"DSN=sqlite:///var/lib/sqlite/db.sqlite?_fk=true&mode=rwc",
				"SELFSERVICE_DEFAULT_BROWSER_RETURN_URL=https://work.withpixie.ai/",
			},
			ExposedPorts: []string{"4433", "4434"},
			// Mounts:       []string{fmt.Sprintf("%s:/etc/config/kratos/", kratosConfDir)},
		}, func(config *docker.HostConfig) {
			config.AutoRemove = true
			config.RestartPolicy = docker.RestartPolicy{Name: "no"}
			config.Mounts = []docker.HostMount{
				{
					Target:   "/var/lib/sqlite",
					Source:   k.volumeName,
					ReadOnly: false,
					Type:     "volume",
				},
			}
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
