package testingutils

import (
	"errors"
	"fmt"
	"testing"
	"time"

	"github.com/cenkalti/backoff/v3"

	"github.com/nats-io/nats-server/v2/server"
	"github.com/nats-io/nats-server/v2/test"
	"github.com/nats-io/nats.go"
	"github.com/phayes/freeport"
)

var testOptions = server.Options{
	Host:   "localhost",
	NoLog:  true,
	NoSigs: true,
}

func startNATS() (gnatsd *server.Server, conn *nats.Conn, err error) {
	defer func() {
		if r := recover(); r != nil {
			err = errors.New("Could not run NATS server")
		}
	}()
	// Find available port.
	port, err := freeport.GetFreePort()
	if err != nil {
		return nil, nil, err
	}

	testOptions.Port = port
	gnatsd = test.RunServer(&testOptions)
	if gnatsd == nil {
		return nil, nil, errors.New("Could not run NATS server")
	}

	url := GetNATSURL(port)
	conn, err = nats.Connect(url)
	if err != nil {
		gnatsd.Shutdown()
		return nil, nil, err
	}

	return gnatsd, conn, nil
}

// StartNATS starts up a NATS server at an open port.
func StartNATS(t *testing.T) (*nats.Conn, func()) {
	var gnatsd *server.Server
	var conn *nats.Conn

	natsConnectFn := func() error {
		var err error
		gnatsd, conn, err = startNATS()
		if err != nil {
			return err
		}
		return nil
	}

	bo := backoff.NewExponentialBackOff()
	bo.MaxInterval = 5 * time.Second
	bo.MaxElapsedTime = 1 * time.Minute

	err := backoff.Retry(natsConnectFn, bo)
	if err != nil {
		t.Fatal("Could not connect to NATS")
	}

	cleanup := func() {
		gnatsd.Shutdown()
		conn.Close()
	}

	return conn, cleanup
}

// GetNATSURL gets the URL of the NATS server.
func GetNATSURL(port int) string {
	return fmt.Sprintf("nats://%s:%d", testOptions.Host, port)
}
