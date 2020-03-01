package testingutils

import (
	"testing"

	"github.com/nats-io/nats-streaming-server/server"
	"github.com/nats-io/stan.go"
)

// StartStan starts up a Stan server with given ID and return client with given ID..
func StartStan(t *testing.T, clusterID, clientID string) (*server.StanServer, stan.Conn, func()) {
	st, err := server.RunServer(clusterID)
	if err != nil {
		t.Fatal(err)
	}

	sc, err := stan.Connect(clusterID, clientID)
	if err != nil {
		t.Fatal(err)
	}

	cleanup := func() {
		st.Shutdown()
	}

	return st, sc, cleanup
}
