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

package events

// This package provides a small wrapper around segment to provide initialization and a dummy
// writer for development mode. The dummy writer is used if segment credentials are not set.

import (
	"sync"

	"github.com/segmentio/analytics-go/v3"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
)

func init() {
	pflag.String("segment_write_key", "", "The key to use for segment")
}

var client analytics.Client
var once sync.Once

type dummyClient struct{}

func (d *dummyClient) Enqueue(msg analytics.Message) error {
	if err := msg.Validate(); err != nil {
		return err
	}

	log.WithField("msg", msg).Debug("Dummy analytics client, dropping message...")
	return nil
}

func (d *dummyClient) Close() error {
	return nil
}

func getDefaultClient() analytics.Client {
	k := viper.GetString("segment_write_key")
	if len(k) > 0 {
		// Key is specified try to to create segment client.
		return analytics.New(k)
	}
	return &dummyClient{}
}

// SetClient sets the default client used for event tracking.
func SetClient(c analytics.Client) {
	client = c
}

// Client returns the client.
func Client() analytics.Client {
	once.Do(func() {
		// client has already been setup.
		if client != nil {
			return
		}
		SetClient(getDefaultClient())
	})
	return client
}
