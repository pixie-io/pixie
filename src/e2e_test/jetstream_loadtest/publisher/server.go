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

package main

import (
	"time"

	"github.com/gofrs/uuid"
	"github.com/prometheus/client_golang/prometheus"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"

	"px.dev/pixie/src/e2e_test/jetstream_loadtest/common"
	"px.dev/pixie/src/e2e_test/util"
	"px.dev/pixie/src/shared/services/msgbus"
	"px.dev/pixie/src/utils"
)

var (
	msgPubOk = prometheus.NewCounterVec(prometheus.CounterOpts{
		Name: "px_msg_pub_ok",
		Help: "The number of msg successes for the channel",
	}, []string{"channel"})

	msgPubErr = prometheus.NewCounterVec(prometheus.CounterOpts{
		Name: "px_msg_pub_error",
		Help: "The number of msg failures for the channel",
	}, []string{"channel"})

	msgPubOkTotal = prometheus.NewCounter(prometheus.CounterOpts{
		Name: "TOTAL_px_msg_pub_ok",
		Help: "The total number of msg successes",
	})

	msgPubErrTotal = prometheus.NewCounter(prometheus.CounterOpts{
		Name: "TOTAL_px_msg_pub_err",
		Help: "The total number of msg errors",
	})

	chanCreateOk = prometheus.NewCounter(prometheus.CounterOpts{
		Name: "px_chan_create_ok",
		Help: "The number of channels",
	})

	chanCreateErr = prometheus.NewCounter(prometheus.CounterOpts{
		Name: "px_chan_create_err",
		Help: "The number of channel create errors",
	})

	startTime = prometheus.NewGauge(prometheus.GaugeOpts{
		Name: "TIME_px_pub_start_time",
		Help: "Start time in unix sec",
	})

	currentTime = prometheus.NewGauge(prometheus.GaugeOpts{
		Name: "TIME_px_pub_current_time",
		Help: "Current time in unix sec",
	})
)

func init() {
	// These defaults are setup to replicate observed loads.
	pflag.Int("num_channels", 4000, "The number of separate message channels")
	pflag.Int("num_msg_per_channel_per_day", 20000, "The number of messages each channel sends per day")
	pflag.Int("msg_size", 512, "Message size in bytes")

	prometheus.MustRegister(msgPubOk)
	prometheus.MustRegister(msgPubErr)
	prometheus.MustRegister(msgPubOkTotal)
	prometheus.MustRegister(msgPubErrTotal)
	prometheus.MustRegister(chanCreateOk)
	prometheus.MustRegister(chanCreateErr)

	prometheus.MustRegister(startTime)
	prometheus.MustRegister(currentTime)
}

type loadGen struct {
	st msgbus.Streamer

	numChannels            int
	numMsgPerChannelPerDay int
	msgSize                int

	done chan struct{}
}

func (g *loadGen) start() {
	startTime.SetToCurrentTime()
	go func() {
		ticker := time.NewTicker(5 * time.Second)
		defer ticker.Stop()
		for {
			select {
			case <-g.done:
				return
			case <-ticker.C:
				currentTime.SetToCurrentTime()
			}
		}
	}()

	for i := 0; i < g.numChannels; i++ {
		name := uuid.Must(uuid.NewV4())
		go g.newChannel(name)
		time.Sleep(common.ChannelCreateDelay)
	}

	// Block until done closes.
	<-g.done
}

func (g *loadGen) newChannel(name uuid.UUID) {
	c := name.String()
	log.WithField("channel", c).Info("creating channel")

	cPb := utils.ProtoFromUUID(name)
	cData, err := cPb.Marshal()
	if err != nil {
		log.WithField("channel", c).WithError(err).Error("Failed to marshal")
		return
	}

	err = g.st.Publish(common.ChannelTrackTopic, cData)
	if err != nil {
		log.WithField("channel", c).WithError(err).Error("Failed to publish new channel")
		chanCreateErr.Inc()
		return
	}

	chanCreateOk.Inc()

	count := 0
	ticker := time.NewTicker(time.Duration(24 * int64(time.Hour) / int64(g.numMsgPerChannelPerDay)))
	defer ticker.Stop()
	for {
		select {
		case <-g.done:
			return
		case <-ticker.C:

			err := g.st.Publish(common.GetTopicForChannel(c), []byte(util.RandPrintable(g.msgSize)))
			if err != nil {
				msgPubErr.WithLabelValues(c).Inc()
				msgPubErrTotal.Inc()
				log.WithField("channel", c).WithError(err).Error("Failed to publish msg")
				continue
			}

			msgPubOk.WithLabelValues(c).Inc()
			msgPubOkTotal.Inc()

			count++
			if count%common.LogEveryN == 0 {
				log.WithField("channel", c).Infof("sent count: %d", count)
			}
		}
	}
}

func main() {
	st := common.SetupJetStream(true)

	numChannels := viper.GetInt("num_channels")
	numMsgPerChannelPerDay := viper.GetInt("num_msg_per_channel_per_day")
	msgSize := viper.GetInt("msg_size")

	gen := loadGen{
		st:                     st,
		numChannels:            numChannels,
		numMsgPerChannelPerDay: numMsgPerChannelPerDay,
		msgSize:                msgSize,
		done:                   make(chan struct{}),
	}

	gen.start()
}
