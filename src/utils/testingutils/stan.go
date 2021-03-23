package testingutils

import (
	"errors"
	"fmt"
	"testing"
	"time"

	"github.com/cenkalti/backoff/v3"
	"github.com/nats-io/nats-server/v2/test"
	"github.com/nats-io/nats-streaming-server/server"
	"github.com/nats-io/stan.go"
	"github.com/phayes/freeport"
)

func startStan(clusterID, clientID string) (srv *server.StanServer, sc stan.Conn, err error) {
	defer func() {
		if r := recover(); r != nil {
			err = errors.New("Could not run STAN server")
		}
	}()

	// Find available port.
	port, err := freeport.GetFreePort()
	if err != nil {
		return nil, nil, err
	}

	opts := test.DefaultTestOptions
	opts.Port = port

	natsOptions := test.DefaultTestOptions
	natsOptions.Port = port
	var serverOptions = server.GetDefaultOptions()
	serverOptions.ID = clusterID

	srv, err = server.RunServerWithOpts(serverOptions, &natsOptions)
	if err != nil {
		return nil, nil, err
	}

	natsURL := fmt.Sprintf("nats://%s:%d", natsOptions.Host, port)
	sc, err = stan.Connect(clusterID, clientID, stan.NatsURL(natsURL))
	if err != nil {
		return nil, nil, err
	}

	return srv, sc, nil
}

// MustStartTestStan starts up a test version of STAN server with given ID and return client with given ID.
func MustStartTestStan(t *testing.T, clusterID, clientID string) (*server.StanServer, stan.Conn, func()) {
	var st *server.StanServer
	var sc stan.Conn

	stanConnectFn := func() error {
		var err error
		st, sc, err = startStan(clusterID, clientID)
		if err != nil {
			return err
		}
		return nil
	}

	bo := backoff.NewExponentialBackOff()
	bo.MaxInterval = 5 * time.Second
	bo.MaxElapsedTime = 1 * time.Minute

	err := backoff.Retry(stanConnectFn, bo)
	if err != nil {
		t.Fatal("Could not connect to NATS")
	}

	cleanup := func() {
		st.Shutdown()
		sc.Close()
	}

	return st, sc, cleanup
}
