package testingutils

import (
	"testing"

	natsserver "github.com/nats-io/nats-server/v2/server"
	"github.com/nats-io/nats-streaming-server/server"
	"github.com/nats-io/stan.go"
	"github.com/phayes/freeport"
)

// StartStan starts up a STAN server with given ID and return client with given ID..
func StartStan(t *testing.T, clusterID, clientID string) (*server.StanServer, stan.Conn, func()) {
	// Find available port.
	port, err := freeport.GetFreePort()
	if err != nil {
		t.Fatal(err)
	}

	var natsOptions = &natsserver.Options{
		Host:   "localhost",
		Port:   port,
		NoLog:  true,
		NoSigs: true,
	}
	var serverOptions = server.GetDefaultOptions()
	serverOptions.ID = clusterID

	st, err := server.RunServerWithOpts(serverOptions, natsOptions)
	if err != nil {
		t.Fatal(err)
	}

	sc, err := stan.Connect(clusterID, clientID, stan.NatsURL(GetNATSURL(port)))
	if err != nil {
		t.Fatal(err)
	}

	cleanup := func() {
		st.Shutdown()
	}

	return st, sc, cleanup
}
