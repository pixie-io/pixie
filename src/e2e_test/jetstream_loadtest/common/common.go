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

package common

import (
	"fmt"
	"net/http"
	"time"

	"github.com/nats-io/nats.go"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"

	"px.dev/pixie/src/shared/services/metrics"
	"px.dev/pixie/src/shared/services/msgbus"
)

const (
	// LogEveryN controls how frequently common log messages are logged.
	LogEveryN = 100

	// ChannelCreateDelay controls the rate at which channels are created.
	ChannelCreateDelay = 20 * time.Millisecond

	// ChannelTrackTopic is the topic tp track channel join/leaves.
	ChannelTrackTopic = "Pixie.JetStream.Loadtest.Channels"

	// UpdateTopic is the topic for messages on a particular channel.
	UpdateTopic = "Pixie.JetStream.Loadtest.Update"
)

// LoadTestStream is the stream config for loadtest messages.
var LoadTestStream = &nats.StreamConfig{
	Name: "LoadTestStream",
	Subjects: []string{
		ChannelTrackTopic,
		GetTopicForChannel("*"),
	},
	Replicas:    5,
	AllowDirect: true,
	MaxAge:      time.Hour * 24,
}

// GetTopicForChannel gets the topic name given a channel name.
func GetTopicForChannel(channel string) string {
	return fmt.Sprintf("%s.%s", UpdateTopic, channel)
}

// SetupJetStream parses the given flags, connects to nats/jetstream and
// returns the streamer interface.
func SetupJetStream(purge bool) msgbus.Streamer {
	pflag.Parse()
	viper.AutomaticEnv()
	viper.SetEnvPrefix("PL")
	viper.BindPFlags(pflag.CommandLine)

	nc := msgbus.MustConnectNATS()
	js := msgbus.MustConnectJetStream(nc)

	// Cap replica count.
	if LoadTestStream.Replicas > len(nc.Servers()) {
		LoadTestStream.Replicas = len(nc.Servers())
	}

	if purge {
		err := js.PurgeStream(LoadTestStream.Name)
		if err != nil {
			log.WithError(err).Warn("Failed to purge stream")
		}
		err = js.DeleteStream(LoadTestStream.Name)
		if err != nil {
			log.WithError(err).Warn("Failed to delete stream")
		}
	}

	strmr, err := msgbus.NewJetStreamStreamer(nc, js, LoadTestStream)
	if err != nil {
		log.WithError(err).Fatal("Could not connect to streamer")
	}

	go func() {
		mux := http.NewServeMux()
		metrics.MustRegisterMetricsHandlerNoDefaultMetrics(mux)
		log.Fatal(http.ListenAndServe(":8080", mux))
	}()

	return strmr
}
