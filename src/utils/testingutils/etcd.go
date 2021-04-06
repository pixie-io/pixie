package testingutils

import (
	"fmt"
	"time"

	"github.com/ory/dockertest/v3"
	"github.com/ory/dockertest/v3/docker"
	log "github.com/sirupsen/logrus"
	clientv3 "go.etcd.io/etcd/client/v3"
)

// SetupEtcd starts up an embedded etcd server on some free ports.
func SetupEtcd() (*clientv3.Client, func()) {
	pool, err := dockertest.NewPool("")
	if err != nil {
		log.Fatalf("Could not connect to docker: %s", err)
	}
	resource, err := pool.RunWithOptions(&dockertest.RunOptions{
		Repository: "quay.io/coreos/etcd",
		Tag:        "v3.3.18",
		// It's safe to hardcode these ports because they are local to the Docker environment.
		Cmd: []string{"/usr/local/bin/etcd",
			"--data-dir=/etcd-data",
			"--name=node1",
			"--initial-advertise-peer-urls=http://0.0.0.0:2380",
			"--listen-peer-urls=http://0.0.0.0:2380",
			"--advertise-client-urls=http://0.0.0.0:2379",
			"--listen-client-urls=http://0.0.0.0:2379",
			"--initial-cluster=node1=http://0.0.0.0:2380",
		},
	}, func(config *docker.HostConfig) {
		config.AutoRemove = true
		config.RestartPolicy = docker.RestartPolicy{Name: "no"}
	})
	if err != nil {
		log.Fatal(err)
	}
	// Set a 5 minute expiration on resources.
	err = resource.Expire(300)
	if err != nil {
		log.Fatal(err)
	}

	clientPort := resource.GetPort("2379/tcp")

	var client *clientv3.Client
	if err = pool.Retry(func() (err error) {
		client, err = clientv3.New(clientv3.Config{
			Endpoints:   []string{fmt.Sprintf("http://localhost:%s", clientPort)},
			DialTimeout: 5 * time.Second,
		})
		if err != nil {
			log.Errorf("Failed to connect to etcd: #{err}")
		}
		return err
	}); err != nil {
		log.Fatalf("Cannot start etcd: %v", err)
	}

	cleanup := func() {
		client.Close()
		if err := pool.Purge(resource); err != nil {
			log.Fatalf("Could not purge resource: %s", err)
		}
	}

	return client, cleanup
}
