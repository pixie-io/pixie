package testingutils

import (
	"errors"
	"fmt"
	"testing"

	"github.com/nats-io/gnatsd/server"
	"github.com/nats-io/gnatsd/test"
	"github.com/nats-io/go-nats"
	"github.com/phayes/freeport"
)

var testOptions = server.Options{
	Host:   "localhost",
	NoLog:  true,
	NoSigs: true,
}

func startNATS() (gnatsd *server.Server, conn *nats.Conn, port int, err error) {
	defer func() {
		if r := recover(); r != nil {
			err = errors.New("Could not run NATS server")
		}
	}()
	// Find available port.
	port, err = freeport.GetFreePort()
	if err != nil {
		return nil, nil, 0, err
	}

	testOptions.Port = port
	gnatsd = test.RunServer(&testOptions)
	if gnatsd == nil {
		return nil, nil, 0, errors.New("Could not run NATS server")
	}

	url := GetNATSURL(port)
	conn, err = nats.Connect(url)
	if err != nil {
		gnatsd.Shutdown()
		return nil, nil, 0, err
	}

	return gnatsd, conn, port, nil
}

// StartNATS starts up a NATS server at an open port.
func StartNATS(t *testing.T) (int, func()) {
	tries := 0

	var gnatsd *server.Server
	var conn *nats.Conn
	var port int
	var err error

	for {
		tries++
		if tries == 5 {
			t.Fatal("Could not connect to NATS")
		}

		gnatsd, conn, port, err = startNATS()
		if err != nil {
			continue
		}

		break
	}

	cleanup := func() {
		gnatsd.Shutdown()
		conn.Close()
	}

	return port, cleanup
}

// GetNATSURL gets the URL of the NATS server.
func GetNATSURL(port int) string {
	return fmt.Sprintf("nats://%s:%d", testOptions.Host, port)
}
