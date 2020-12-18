package testingutils

import (
	"fmt"

	"github.com/olivere/elastic/v7"
	"github.com/ory/dockertest"
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
		},
	})
	if err != nil {
		return nil, cleanup, err
	}
	// Set a 5 minute expiration on resources.
	resource.Expire(300)

	clientPort := resource.GetPort("9200/tcp")
	var client *elastic.Client
	err = pool.Retry(func() (err error) {
		client, err = connectElastic(fmt.Sprintf("http://localhost:%s", clientPort), "elastic", esPass)
		if err != nil {
			log.WithError(err).Errorf("Failed to connect to elasticsearch.")
		}
		return err
	})
	if err != nil {
		pool.Purge(resource)
		return nil, cleanup, fmt.Errorf("Cannot start elasticsearch: %s", err)
	}

	log.Info("Successfully connected to elastic.")

	cleanup = func() {
		client.Stop()
		pool.Purge(resource)
	}

	return client, cleanup, nil
}
