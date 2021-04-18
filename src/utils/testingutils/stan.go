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
