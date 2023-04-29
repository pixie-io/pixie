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
	"net"
	"net/url"
	"os"
	"time"

	log "github.com/sirupsen/logrus"
	clientv3 "go.etcd.io/etcd/client/v3"
	"go.etcd.io/etcd/server/v3/embed"
	"go.uber.org/zap/zapcore"
)

// SetupEtcd starts up an embedded etcd server on some free ports.
func SetupEtcd() (*clientv3.Client, func(), error) {
	cfg := embed.NewConfig()

	dir, err := os.MkdirTemp("", "etcd")
	if err != nil {
		return nil, nil, err
	}
	cfg.Dir = dir
	cfg.InitialElectionTickAdvance = false
	cfg.TickMs = 10
	cfg.ElectionMs = 50
	cfg.LogLevel = zapcore.ErrorLevel.String()

	// Find and immediately free an empty port.
	// Hope that it doesn't get recycled before etcd starts.
	listener, err := net.Listen("tcp", "localhost:0")
	if err != nil {
		return nil, nil, err
	}
	err = listener.Close()
	if err != nil {
		return nil, nil, err
	}

	clientURL, err := url.Parse(fmt.Sprintf("http://%s", listener.Addr().String()))
	if err != nil {
		return nil, nil, err
	}

	cfg.ListenPeerUrls = []url.URL{}
	cfg.ListenClientUrls = []url.URL{*clientURL}

	e, err := embed.StartEtcd(cfg)
	if err != nil {
		return nil, nil, err
	}

	select {
	case <-e.Server.ReadyNotify():
		log.Debug("Server is ready!")
	case <-time.After(60 * time.Second):
		e.Server.Stop() // trigger a shutdown
		log.Error("Server took too long to start!")
		return nil, nil, errors.New("timed out waiting for etcd")
	}

	client, err := clientv3.New(clientv3.Config{
		Endpoints:   []string{clientURL.String()},
		DialTimeout: 5 * time.Second,
	})
	if err != nil {
		return nil, nil, err
	}

	cleanup := func() {
		client.Close()
		e.Close()
	}

	return client, cleanup, nil
}
